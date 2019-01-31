#include "cross_nat_connector.h"

#include <functional>
#include <tuple>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "outgoing_tunnel_connection_watcher.h"
#include "outgoing_tunnel_pool.h"
#include "udp/connector.h"
#include "../cloud_connect_settings.h"

namespace nx {
namespace network {
namespace cloud {

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
    const AddressEntry& targetPeerAddress,
    std::optional<hpm::api::MediatorAddress> mediatorAddress)
    :
    m_cloudConnectController(cloudConnectController),
    m_targetPeerAddress(targetPeerAddress),
    m_connectSessionId(QnUuid::createUuid().toByteArray().toStdString()),
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
            if (timeout > std::chrono::milliseconds::zero())
                m_connectTimeout = timeout;

            fetchMediatorUdpEndpoint();
        });
}

QString CrossNatConnector::getRemotePeerName() const
{
    return m_remotePeerFullName;
}

SocketAddress CrossNatConnector::localUdpHolePunchingEndpoint() const
{
    return m_localAddress;
}

void CrossNatConnector::replaceOriginatingHostAddress(const QString& address)
{
    m_originatingHostAddressReplacement = address;
}

utils::ResultCounter<nx::hpm::api::ResultCode>& CrossNatConnector::mediatorResponseCounter()
{
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

utils::ResultCounter<nx::hpm::api::ResultCode>
    CrossNatConnector::s_mediatorResponseCounter(
        static_cast<QString(*)(nx::hpm::api::ResultCode)>(&nx::hpm::api::toString));

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

    auto mediatorUdpClient = prepareForUdpHolePunching();
    if (!mediatorUdpClient)
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_WARNING(this, lm("cross-nat %1. Failed to prepare mediator udp client. %2")
            .args(m_connectSessionId, SystemError::getLastOSErrorText()));
        post(
            [this, errorCode]() mutable
            {
                s_mediatorResponseCounter.addResult(hpm::api::ResultCode::badTransport);
                nx::utils::swapAndCall(m_completionHandler, errorCode, nullptr);
            });
        return;
    }

    m_localAddress = mediatorUdpClient->socket()->getLocalAddress();

    NX_VERBOSE(this, lm("cross-nat %1. Connecting to %2 with timeout %3, from local port %4")
        .args(m_connectSessionId, m_targetPeerAddress.host.toString(),
            m_connectTimeout, m_localAddress.port));

    if (m_connectTimeout)
        m_timer->start(*m_connectTimeout, [this]() { onTimeout(); });

    m_connectResultReport.resultCode =
        api::NatTraversalResultCode::noResponseFromMediator;

    if (!m_connectionMediationInitiator)
    {
        m_connectionMediationInitiator =
            std::make_unique<ConnectionMediationInitiator>(
                m_cloudConnectController->settings(),
                *m_mediatorAddress,
                std::move(mediatorUdpClient));
    }

    m_connectionMediationInitiator->start(
        prepareConnectRequest(m_localAddress),
        [this](auto&&... args) { onConnectResponse(std::move(args)...); });
}

std::unique_ptr<hpm::api::MediatorClientUdpConnection> 
    CrossNatConnector::prepareForUdpHolePunching()
{
    auto mediatorUdpClient =
        std::make_unique<api::MediatorClientUdpConnection>(
            m_mediatorAddress->stunUdpEndpoint);
    mediatorUdpClient->bindToAioThread(getAioThread());

    if (!mediatorUdpClient->socket()->setReuseAddrFlag(true) ||
        !mediatorUdpClient->socket()->bind(SocketAddress::anyAddress))
    {
        return nullptr;
    }

    return mediatorUdpClient;
}

void CrossNatConnector::onConnectResponse(
    api::ResultCode resultCode,
    api::ConnectResponse response)
{
    using namespace std::placeholders;

    s_mediatorResponseCounter.addResult(resultCode);
    NX_VERBOSE(this, lm("cross-nat %1. Received %2 response from mediator")
        .args(m_connectSessionId, QnLexical::serialized(resultCode)));

    if (m_done)
        return;

    m_mediatorAddress->stunUdpEndpoint =
        m_connectionMediationInitiator->mediatorLocation().stunUdpEndpoint;

    m_timer->cancelSync();

    auto mediatorClientSocket = m_connectionMediationInitiator->takeUdpSocket();
    m_connectionMediationInitiator.reset();

    if (resultCode != api::ResultCode::ok)
    {
        auto completionHandler = std::move(m_completionHandler);
        completionHandler(mediatorResultToSysErrorCode(resultCode), nullptr);
        return;
    }

    const auto effectiveConnectTimeout = calculateTimeLeftForConnect();
    m_connectionParameters = response.params;
    m_remotePeerFullName = response.destinationHostFullName;

    m_cloudConnectorExecutor = std::make_unique<ConnectorExecutor>(
        m_targetPeerAddress,
        m_connectSessionId,
        response,
        std::move(mediatorClientSocket));
    m_cloudConnectorExecutor->setTimeout(effectiveConnectTimeout);
    m_cloudConnectorExecutor->start(
        std::bind(&CrossNatConnector::onConnectorFinished, this, _1, _2, _3));
}

std::chrono::milliseconds CrossNatConnector::calculateTimeLeftForConnect()
{
    using namespace std::chrono;

    milliseconds effectiveConnectTimeout(0);
    if (m_connectTimeout)
    {
        if (const auto timeToEvent = m_timer->timeToEvent())
            effectiveConnectTimeout = duration_cast<milliseconds>(*timeToEvent);

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
    NX_VERBOSE(this, lm("cross-nat %1. Connector has finished with result: %2, %3")
        .args(m_connectSessionId, QnLexical::serialized(resultCode),
            SystemError::toString(sysErrorCode)));

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
    NX_DEBUG(this, lm("cross-nat %1 timed out. Result code %2")
        .args(m_connectSessionId, QnLexical::serialized(m_connectResultReport.resultCode)));

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

    NX_VERBOSE(this, lm("cross-nat %1. result: %2, system result code: %3")
        .args(m_connectSessionId, QnLexical::serialized(resultCode),
            SystemError::toString(sysErrorCode)));

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
    m_connectResultReport.connectSessionId = m_connectSessionId.c_str();
    m_connectResultReport.resultCode = resultCode;

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
        NX_DEBUG(this, lm("cross-nat %1. Failed to send report to mediator. %2")
            .args(m_connectSessionId, SystemError::toString(errorCode)));
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
    NX_VERBOSE(this, lm("cross-nat %1. report send result code: %2. "
        "Invoking handler with result: %3")
        .args(m_connectSessionId, SystemError::toString(errorCode),
            SystemError::toString(sysErrorCodeToReport)));

    if (m_connection)
    {
        NX_INFO(this, lm("Cloud connection (session %1) to %2 has been established. Info %3")
            .args(m_connectSessionId, m_targetPeerAddress.toString(), m_connection->toString()));
    }
    else
    {
        NX_INFO(this, lm("Cloud connection (session %1) to %2 has failed. %3 / %4")
            .args(m_connectSessionId, m_targetPeerAddress.toString(),
                QnLexical::serialized(m_connectResultReport.resultCode),
                SystemError::toString(m_connectResultReport.sysErrorCode)));
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
    connectRequest.connectSessionId = m_connectSessionId.c_str();
    connectRequest.connectionMethods = 
        api::ConnectionMethod::udpHolePunching | api::ConnectionMethod::proxy;
    connectRequest.destinationHostName = m_targetPeerAddress.host.toString().toUtf8();
    if (m_originatingHostAddressReplacement)
    {
        connectRequest.ignoreSourceAddress = true;
        connectRequest.udpEndpointList.emplace_back(
            SocketAddress(*m_originatingHostAddressReplacement, 0));
        //< In case of zero port mediator will take request source port.
    }

    // Adding local interfaces IP.
    const auto localInterfaceList = getAllIPv4Interfaces();
    for (const auto& addr: localInterfaceList)
    {
        connectRequest.udpEndpointList.push_back(
            SocketAddress(addr.address.toString(), udpHolePunchingLocalEndpoint.port));
    }

    return connectRequest;
}

} // namespace cloud
} // namespace network
} // namespace nx
