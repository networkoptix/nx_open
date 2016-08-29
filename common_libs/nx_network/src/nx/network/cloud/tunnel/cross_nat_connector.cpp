/**********************************************************
* Jul 7, 2016
* akolesnikov
***********************************************************/

#include "cross_nat_connector.h"

#include <nx/fusion/serialization/lexical.h>

#include "nx/network/socket_global.h"
#include <nx/network/cloud/tunnel/tcp/outgoing_reverse_tunnel_connection.h>
#include "udp/connector.h"


namespace nx {
namespace network {
namespace cloud {

using namespace nx::hpm;

namespace {
api::NatTraversalResultCode mediatorResultToHolePunchingResult(
    api::ResultCode resultCode)
{
    switch (resultCode)
    {
        case api::ResultCode::ok:
            return api::NatTraversalResultCode::ok;
        case api::ResultCode::networkError:
        case api::ResultCode::timedOut:
            return api::NatTraversalResultCode::noResponseFromMediator;
        default:
            return api::NatTraversalResultCode::mediatorReportedError;
    }
}

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
}   // namespace

CrossNatConnector::CrossNatConnector(
    const AddressEntry& targetPeerAddress,
    boost::optional<SocketAddress> mediatorAddress)
:
    m_targetPeerAddress(targetPeerAddress),
    m_connectSessionId(QnUuid::createUuid().toByteArray()),
    m_mediatorAddress(
        mediatorAddress
        ? mediatorAddress.get()
        : *nx::network::SocketGlobals::mediatorConnector().mediatorAddress()),
    m_originatingHostAddressReplacement(
        SocketGlobals::cloudConnectSettings().originatingHostAddressReplacement()),
    m_done(false)
{
    m_mediatorUdpClient = 
        std::make_unique<api::MediatorClientUdpConnection>(m_mediatorAddress);
    m_mediatorUdpClient->socket()->bindToAioThread(getAioThread());
}

CrossNatConnector::~CrossNatConnector()
{
    stopWhileInAioThread();
}

void CrossNatConnector::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    BaseType::bindToAioThread(aioThread);
    m_mediatorUdpClient->socket()->bindToAioThread(aioThread);
}

void CrossNatConnector::stopWhileInAioThread()
{
    m_mediatorUdpClient.reset();
    m_connectors.clear();
    m_connectResultReportSender.reset();
}

void CrossNatConnector::connect(
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    //invokeInAioThread(&CrossNatConnector::connectInternal, this, timeout, std::move(handler));

    post(
        [this, timeout, handler = std::move(handler)]() mutable
        {
            auto holder = SocketGlobals::tcpReversePool().getConnectionHolder(
                m_targetPeerAddress.host.toString().toUtf8());

            if (!holder->hasConnections())
            {
                return handler(
                    SystemError::noError,
                    std::make_unique<tcp::OutgoingReverseTunnelConnection>(
                        nullptr, //< TODO: find out which aioThread shell be used
                        std::move(holder)));
            }

            issueConnectRequestToMediator(timeout, std::move(handler));
        });
}

SocketAddress CrossNatConnector::localAddress() const
{
    return m_localAddress;
}

void CrossNatConnector::replaceOriginatingHostAddress(const QString& address)
{
    m_originatingHostAddressReplacement = address;
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

void CrossNatConnector::issueConnectRequestToMediator(
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    if (!m_mediatorUdpClient->socket()->setReuseAddrFlag(true) ||
        !m_mediatorUdpClient->socket()->bind(SocketAddress::anyAddress))
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("cross-nat %1. Failed to bind to mediator udp client to local port. %2")
            .arg(m_connectSessionId).arg(SystemError::getLastOSErrorText()),
            cl_logWARNING);
        post(
            [handler = move(handler), errorCode]() mutable
            {
                handler(errorCode, nullptr);
            });
        return;
    }

    m_localAddress = m_mediatorUdpClient->socket()->getLocalAddress();

    NX_LOGX(lm("cross-nat %1. connecting to %2 with timeout %3, from local port %4")
        .arg(m_connectSessionId).arg(m_targetPeerAddress.host.toString())
        .arg(timeout).arg(m_mediatorUdpClient->socket()->getLocalAddress().port),
        cl_logDEBUG2);

    if (timeout > std::chrono::milliseconds::zero())
    {
        m_connectTimeout = timeout;
        timer()->start(
            timeout,
            std::bind(&CrossNatConnector::onTimeout, this));
    }

    m_completionHandler = std::move(handler);

    m_connectResultReport.resultCode =
        api::NatTraversalResultCode::noResponseFromMediator;

    api::ConnectRequest connectRequest;
    connectRequest.originatingPeerID = QnUuid::createUuid().toByteArray();
    connectRequest.connectSessionId = m_connectSessionId;
    connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
    connectRequest.destinationHostName = m_targetPeerAddress.host.toString().toUtf8();
    if (m_originatingHostAddressReplacement)
    {
        connectRequest.ignoreSourceAddress = true;
        connectRequest.udpEndpointList.emplace_back(
            SocketAddress(*m_originatingHostAddressReplacement, 0));    //in case of zero port mediator will take request source port
    }

    //adding local interfaces IP
    const auto localInterfaceList = getAllIPv4Interfaces();
    for (const auto& addr: localInterfaceList)
    {
        connectRequest.udpEndpointList.push_back(
            SocketAddress(addr.address.toString(), m_localAddress.port));
    }

    using namespace std::placeholders;
    m_mediatorUdpClient->connect(
        std::move(connectRequest),
        std::bind(&CrossNatConnector::onConnectResponse, this, _1, _2));
}

void CrossNatConnector::onConnectResponse(
    api::ResultCode resultCode,
    api::ConnectResponse response)
{
    if (m_done)
        return;

    timer()->cancelSync();

    //TODO #ak add support for redirect to another mediator instance

    if (resultCode != api::ResultCode::ok)
    {
        m_mediatorUdpClient.reset();
        auto completionHandler = std::move(m_completionHandler);
        completionHandler(mediatorResultToSysErrorCode(resultCode), nullptr);
        return;
    }

    using namespace std::chrono;
    milliseconds effectiveConnectTimeout(0);
    if (m_connectTimeout)
    {
        effectiveConnectTimeout = duration_cast<milliseconds>(timer()->timeToEvent());
        if (effectiveConnectTimeout == milliseconds::zero())
            effectiveConnectTimeout = milliseconds(1);   //zero timeout is infinity
    }

    //creating corresponding connectors
    m_connectors = ConnectorFactory::createCloudConnectors(
        m_targetPeerAddress,
        m_connectSessionId,
        response,
        std::move(m_mediatorUdpClient->takeSocket()));
    //TODO #ak sorting connectors by priority
    m_mediatorUdpClient.reset();
    for (auto it = m_connectors.begin(); it != m_connectors.end(); ++it)
    {
        (*it)->bindToAioThread(getAioThread());
        (*it)->connect(
            response,
            effectiveConnectTimeout,
            [it, this](
                api::NatTraversalResultCode resultCode,
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
            {
                onConnectorFinished(
                    it,
                    resultCode,
                    errorCode,
                    std::move(connection));
            });
    }
}

void CrossNatConnector::onConnectorFinished(
    ConnectorFactory::CloudConnectors::iterator connectorIter,
    api::NatTraversalResultCode resultCode,
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
{
    auto connector = std::move(*connectorIter);
    m_connectors.erase(connectorIter);

    if (resultCode != api::NatTraversalResultCode::ok && !m_connectors.empty())
        return;     //waiting for other connectors to complete

    NX_CRITICAL((resultCode != api::NatTraversalResultCode::ok) || connection);
    m_connectors.clear();   //cancelling other connectors
    m_connection = std::move(connection);
    holePunchingDone(resultCode, sysErrorCode);
}

void CrossNatConnector::onTimeout()
{
    NX_LOGX(lm("cross-nat %1 timed out. Result code %2")
        .arg(QnLexical::serialized(m_connectResultReport.resultCode)),
        cl_logDEBUG1);

    m_done = true;

    //removing connectors
    m_mediatorUdpClient.reset();
    m_connectors.clear();

    //reporting failure
    holePunchingDone(
        m_connectResultReport.resultCode,
        SystemError::timedOut);
}

void CrossNatConnector::holePunchingDone(
    api::NatTraversalResultCode resultCode,
    SystemError::ErrorCode sysErrorCode)
{
    NX_LOGX(lm("cross-nat %1. result: %2, system result code: %3")
        .arg(m_connectSessionId).arg(QnLexical::serialized(resultCode))
        .arg(SystemError::toString(sysErrorCode)),
        cl_logDEBUG2);

    //we are in aio thread
    timer()->cancelSync();

    m_connectResultReport.sysErrorCode = sysErrorCode;
    if (resultCode == api::NatTraversalResultCode::noResponseFromMediator)
    {
        //not sending report to mediator since no answer from mediator...
        return connectSessionReportSent(SystemError::noError);
    }

    //reporting result to mediator
    //after message has been sent - reporting result to client
    m_connectResultReport.connectSessionId = m_connectSessionId;
    m_connectResultReport.resultCode = resultCode;

    using namespace std::placeholders;
    m_connectResultReportSender = std::make_unique<stun::UnreliableMessagePipeline>(this);
    m_connectResultReportSender->bindToAioThread(getAioThread());
    stun::Message connectResultReportMessage(
        stun::Header(
            stun::MessageClass::request,
            stun::cc::methods::connectionResult));
    m_connectResultReport.serialize(&connectResultReportMessage);
    m_connectResultReportSender->sendMessage(
        m_mediatorAddress,
        std::move(connectResultReportMessage),
        std::bind(&CrossNatConnector::connectSessionReportSent, this, _1));
}

void CrossNatConnector::connectSessionReportSent(
    SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("cross-nat %1. Failed to send report to mediator. %2")
            .arg(m_connectSessionId).arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);
    }

    //ignoring send report result code
    SystemError::ErrorCode sysErrorCodeToReport = SystemError::noError;
    if (m_connectResultReport.resultCode != api::NatTraversalResultCode::ok)
    {
        sysErrorCodeToReport =
            m_connectResultReport.sysErrorCode == SystemError::noError
            ? SystemError::connectionReset
            : m_connectResultReport.sysErrorCode;
    }
    NX_LOGX(lm("cross-nat %1. report send result code: %2. Invoking handler with result: %3")
        .arg(m_connectSessionId).arg(SystemError::toString(errorCode))
        .arg(SystemError::toString(sysErrorCodeToReport)),
        cl_logDEBUG2);

    auto completionHandler = std::move(m_completionHandler);
    auto tunnelConnection = std::move(m_connection);
    completionHandler(sysErrorCodeToReport, std::move(tunnelConnection));
}

}   // namespace cloud
}   // namespace network
}   // namespace nx
