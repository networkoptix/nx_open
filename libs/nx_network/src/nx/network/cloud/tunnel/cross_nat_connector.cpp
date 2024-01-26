// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_nat_connector.h"

#include <functional>
#include <tuple>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "outgoing_tunnel_connection_watcher.h"
#include "outgoing_tunnel_pool.h"
#include "udp/connector.h"
#include "../cloud_connect_settings.h"

namespace nx::network::cloud {

using namespace nx::hpm;

namespace {

SystemError::ErrorCode mediatorResultToSysErrorCode(api::ResultCode resultCode)
{
    switch (resultCode)
    {
        case api::ResultCode::ok:
            return SystemError::noError;
        case api::ResultCode::notFound:
            return SystemError::hostNotFound;
        case api::ResultCode::timedOut:
            return SystemError::timedOut;
        default:
            return SystemError::connectionReset;
    }
}

} // namespace

//-------------------------------------------------------------------------------------------------

CrossNatConnector::CrossNatConnector(
    CloudConnectController* cloudConnectController,
    const std::string& connectSessionId,
    const AddressEntry& targetPeerAddress,
    std::optional<hpm::api::MediatorAddress> mediatorAddress)
    :
    m_cloudConnectController(cloudConnectController),
    m_targetPeerAddress(targetPeerAddress),
    m_connectSessionId(connectSessionId),
    m_mediatorAddress(std::move(mediatorAddress)),
    m_originatingHostAddressReplacement(
        m_cloudConnectController->settings().originatingHostAddressReplacement()),
    m_done(false),
    m_mediatorAddressFetcher(
        &nx::hpm::api::AbstractMediatorConnector::fetchAddress)
{
    m_timer = std::make_unique<aio::Timer>();
    m_timer->bindToAioThread(getAioThread());
    m_mediatorAddressFetcher.bindToAioThread(getAioThread());
}

void CrossNatConnector::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    AbstractCrossNatConnector::bindToAioThread(aioThread);

    if (m_connectionMediationInitiator)
        m_connectionMediationInitiator->bindToAioThread(aioThread);
    m_timer->bindToAioThread(aioThread);
    if (m_cloudConnectorExecutor)
        m_cloudConnectorExecutor->bindToAioThread(aioThread);
    m_mediatorAddressFetcher.bindToAioThread(aioThread);
}

void CrossNatConnector::connect(
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    post(
        [this, timeout, handler = std::move(handler)]() mutable
        {
            m_completionHandler = std::move(handler);
            m_connectTimeout = timeout;

            fetchMediatorUdpEndpoint();
        });
}

std::string CrossNatConnector::getRemotePeerName() const
{
    return m_remotePeerFullName;
}

SocketAddress CrossNatConnector::localUdpHolePunchingEndpoint() const
{
    return m_localAddress;
}

void CrossNatConnector::replaceOriginatingHostAddress(const std::string& address)
{
    m_originatingHostAddressReplacement = address;
}

utils::ResultCounter<nx::hpm::api::ResultCode>& CrossNatConnector::mediatorResponseCounter()
{
    static utils::ResultCounter<nx::hpm::api::ResultCode> s_mediatorResponseCounter(
        [](nx::hpm::api::ResultCode code) { return nx::reflect::toString(code); });

    return s_mediatorResponseCounter;
}

void CrossNatConnector::stopWhileInAioThread()
{
    m_connectionMediationInitiator.reset();
    m_connectResultReportSender.reset();
    m_connection.reset();
    m_timer.reset();
    m_cloudConnectorExecutor.reset();
    m_mediatorAddressFetcher.pleaseStopSync();
}

void CrossNatConnector::messageReceived(
    SocketAddress /*sourceAddress*/,
    stun::Message /*msg*/)
{
    //here we can receive response to connect result report. We just don't need it
}

void CrossNatConnector::ioFailure(SystemError::ErrorCode /*errorCode*/)
{
    //if error happens when sending connect result report,
    //  it will be reported to TunnelConnector::connectSessionReportSent too
    //  and we will handle error there
}

void CrossNatConnector::fetchMediatorUdpEndpoint()
{
    if (m_mediatorAddress)
        return issueConnectRequestToMediator();

    m_mediatorAddressFetcher.invoke(
        [this](auto&&... args) { onFetchMediatorAddressCompletion(std::move(args)...); },
        &m_cloudConnectController->mediatorConnector());
}

void CrossNatConnector::onFetchMediatorAddressCompletion(
    http::StatusCode::Value resultCode,
    hpm::api::MediatorAddress mediatorAddress)
{
    if (!http::StatusCode::isSuccessCode(resultCode))
    {
        nx::utils::swapAndCall(m_completionHandler, SystemError::hostUnreachable, nullptr);
        return;
    }

    m_mediatorAddress = std::move(mediatorAddress);
    issueConnectRequestToMediator();
}

void CrossNatConnector::issueConnectRequestToMediator()
{
    NX_ASSERT(isInSelfAioThread());

    auto [resultCode, mediatorUdpClient] = prepareForUdpHolePunching();
    if (!mediatorUdpClient)
    {
        NX_WARNING(this, "cross-nat %1. Failed to prepare mediator udp client. %2",
            m_connectSessionId, SystemError::toString(resultCode));
        post(
            [this, resultCode = resultCode]() mutable
            {
                mediatorResponseCounter().addResult(hpm::api::ResultCode::badTransport);
                nx::utils::swapAndCall(m_completionHandler, resultCode, nullptr);
            });
        return;
    }

    m_localAddress = mediatorUdpClient->socket()->getLocalAddress();

    NX_VERBOSE(this,
        "cross-nat %1. Connecting to %2 with timeout %3, from local port %4",
        m_connectSessionId,
        m_targetPeerAddress.host.toString(),
        m_connectTimeout,
        m_localAddress.port);

    if (m_connectTimeout && m_connectTimeout != nx::network::kNoTimeout)
        m_timer->start(*m_connectTimeout, [this]() { onTimeout(); });

    m_connectResultReport.resultCode =
        api::NatTraversalResultCode::noResponseFromMediator;

    if (!m_connectionMediationInitiator)
    {
        m_connectionMediationInitiator =
            std::make_unique<ConnectionMediationInitiator>(
                m_cloudConnectController->settings(),
                m_connectSessionId,
                *m_mediatorAddress,
                std::move(mediatorUdpClient));

        m_connectionMediationInitiator->setTimeout(m_connectTimeout);
    }

    m_connectionMediationInitiator->start(
        prepareConnectRequest(m_localAddress),
        [this](auto&&... args) { onConnectResponse(std::move(args)...); });
}

std::tuple<SystemError::ErrorCode, std::unique_ptr<hpm::api::MediatorClientUdpConnection>>
    CrossNatConnector::prepareForUdpHolePunching()
{
    auto mediatorUdpClient =
        std::make_unique<api::MediatorClientUdpConnection>(
            m_mediatorAddress->stunUdpEndpoint);
    mediatorUdpClient->bindToAioThread(getAioThread());

    if (!mediatorUdpClient->socket()->setReuseAddrFlag(true) ||
        !mediatorUdpClient->socket()->bind(SocketAddress::anyAddress))
    {
        return std::make_tuple(SystemError::getLastOSErrorCode(), nullptr);
    }

    return std::make_tuple(SystemError::noError, std::move(mediatorUdpClient));
}

void CrossNatConnector::onConnectResponse(
    api::ResultCode resultCode,
    api::ConnectResponse response)
{
    mediatorResponseCounter().addResult(resultCode);
    NX_VERBOSE(this, "cross-nat %1. Received %2 response from mediator",
        m_connectSessionId, resultCode);

    if (m_done)
        return;

    m_mediatorAddress->stunUdpEndpoint =
        m_connectionMediationInitiator->mediatorLocation().stunUdpEndpoint;

    const auto effectiveConnectTimeout = calculateTimeLeftForConnect();
    m_timer->cancelSync();

    auto mediatorClientSocket = m_connectionMediationInitiator->takeUdpSocket();
    m_connectionMediationInitiator.reset();

    if (resultCode != api::ResultCode::ok)
    {
        auto completionHandler = std::move(m_completionHandler);
        completionHandler(mediatorResultToSysErrorCode(resultCode), nullptr);
        return;
    }

    m_connectionParameters = response.params;
    m_remotePeerFullName = response.destinationHostFullName;

    m_cloudConnectorExecutor = std::make_unique<ConnectorExecutor>(
        m_targetPeerAddress,
        m_connectSessionId,
        response,
        std::move(mediatorClientSocket));
    m_cloudConnectorExecutor->setTimeout(effectiveConnectTimeout);
    m_cloudConnectorExecutor->start(
        [this](auto&&... args) { onConnectorFinished(std::forward<decltype(args)>(args)...); });
}

std::chrono::milliseconds CrossNatConnector::calculateTimeLeftForConnect()
{
    using namespace std::chrono;

    milliseconds effectiveConnectTimeout = nx::network::kNoTimeout;
    if (m_connectTimeout && m_connectTimeout != nx::network::kNoTimeout)
    {
        effectiveConnectTimeout =
            duration_cast<milliseconds>(m_timer->timeToEvent().value_or(nanoseconds::zero()));

        if (effectiveConnectTimeout == milliseconds::zero())
            effectiveConnectTimeout = milliseconds(1);   //< Zero timeout is infinity.
    }

    return effectiveConnectTimeout;
}

void CrossNatConnector::onConnectorFinished(
    api::NatTraversalResultCode resultCode,
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
{
    NX_VERBOSE(this, "cross-nat %1. Connector has finished with result: %2, %3",
        m_connectSessionId, resultCode, SystemError::toString(sysErrorCode));

    m_cloudConnectorExecutor.reset();

    if (connection)
    {
        auto tunnelWatcher = std::make_unique<OutgoingTunnelConnectionWatcher>(
            std::move(m_connectionParameters),
            std::move(connection));
        tunnelWatcher->bindToAioThread(getAioThread());
        m_connection = std::move(tunnelWatcher);
    }

    holePunchingDone(resultCode, sysErrorCode);
}

void CrossNatConnector::onTimeout()
{
    NX_DEBUG(this, "cross-nat %1 timed out. Result code %2",
        m_connectSessionId, m_connectResultReport.resultCode);

    m_done = true;

    // Removing connectors.
    m_connectionMediationInitiator.reset();
    m_cloudConnectorExecutor.reset();

    // Reporting failure.
    holePunchingDone(
        m_connectResultReport.resultCode,
        SystemError::timedOut);
}

void CrossNatConnector::holePunchingDone(
    api::NatTraversalResultCode resultCode,
    SystemError::ErrorCode sysErrorCode)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, "cross-nat %1. result: %2, system result code: %3",
        m_connectSessionId, resultCode, SystemError::toString(sysErrorCode));

    // We are in aio thread.
    m_timer->cancelSync();

    m_connectResultReport.sysErrorCode = sysErrorCode;
    if (resultCode == api::NatTraversalResultCode::noResponseFromMediator)
    {
        // Not sending report to mediator since no answer from mediator...
        return connectSessionReportSent(SystemError::noError);
    }

    // Reporting result to mediator.
    // After message has been sent - reporting result to client.
    m_connectResultReport.connectSessionId = m_connectSessionId;
    m_connectResultReport.resultCode = resultCode;
    if (m_connection)
        m_connectResultReport.connectType = m_connection->connectType();

    m_connectResultReportSender = std::make_unique<stun::UnreliableMessagePipeline>(this);
    m_connectResultReportSender->bindToAioThread(getAioThread());
    stun::Message connectResultReportMessage(
        stun::Header(
            stun::MessageClass::request,
            stun::extension::methods::connectionResult));
    m_connectResultReport.serialize(&connectResultReportMessage);
    m_connectResultReportSender->sendMessage(
        m_mediatorAddress->stunUdpEndpoint,
        std::move(connectResultReportMessage),
        std::bind(&CrossNatConnector::connectSessionReportSent, this, _1));
}

void CrossNatConnector::connectSessionReportSent(
    SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, "cross-nat %1. Failed to send report to mediator. %2",
            m_connectSessionId, SystemError::toString(errorCode));
    }

    // Ignoring send report result code.
    SystemError::ErrorCode sysErrorCodeToReport = SystemError::noError;
    if (m_connectResultReport.resultCode != api::NatTraversalResultCode::ok)
    {
        sysErrorCodeToReport =
            m_connectResultReport.sysErrorCode == SystemError::noError
            ? hpm::api::toSystemErrorCode(m_connectResultReport.resultCode)
            : m_connectResultReport.sysErrorCode;
    }
    NX_VERBOSE(this,
        "cross-nat %1. report send result code: %2. Invoking handler with result: %3",
        m_connectSessionId,
        SystemError::toString(errorCode),
        SystemError::toString(sysErrorCodeToReport));

    if (m_connection)
    {
        NX_INFO(this, "Cloud connection (session %1) to %2 has been established. Info %3",
            m_connectSessionId, m_targetPeerAddress.toString(), m_connection->toString());
    }
    else
    {
        NX_INFO(this,
            "Cloud connection (session %1) to %2 has failed. %3 / %4",
            m_connectSessionId,
            m_targetPeerAddress.toString(),
            m_connectResultReport.resultCode,
            SystemError::toString(m_connectResultReport.sysErrorCode));
    }

    auto completionHandler = std::move(m_completionHandler);
    auto tunnelConnection = std::move(m_connection);
    completionHandler(sysErrorCodeToReport, std::move(tunnelConnection));
}

hpm::api::ConnectRequest CrossNatConnector::prepareConnectRequest(
    const SocketAddress& udpHolePunchingLocalEndpoint) const
{
    api::ConnectRequest connectRequest;

    connectRequest.originatingPeerId = m_cloudConnectController->outgoingTunnelPool().ownPeerId();
    connectRequest.connectSessionId = m_connectSessionId;
    connectRequest.connectionMethods =
        api::ConnectionMethod::udpHolePunching | api::ConnectionMethod::proxy;
    connectRequest.destinationHostName = m_targetPeerAddress.host.toString();

    if (m_cloudConnectController->settings().isUdpHpEnabled)
    {
        if (m_originatingHostAddressReplacement)
        {
            connectRequest.ignoreSourceAddress = true;
            connectRequest.udpEndpointList.emplace_back(
                SocketAddress(*m_originatingHostAddressReplacement, 0));
            //< In case of zero port mediator will take request source port.
        }

        // Adding local interfaces IP.
        for (const auto& address: allLocalAddresses(AddressFilter::onlyFirstIpV4))
        {
            connectRequest.udpEndpointList.push_back(
                SocketAddress(address, udpHolePunchingLocalEndpoint.port));
        }
    }

    return connectRequest;
}

} // namespace nx::network::cloud
