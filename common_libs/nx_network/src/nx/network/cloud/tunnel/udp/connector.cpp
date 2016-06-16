/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "connector.h"

#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>

#include <nx/fusion/serialization/lexical.h>

#include "outgoing_tunnel_connection.h"
#include "rendezvous_connector_with_verification.h"


namespace nx {
namespace network {
namespace cloud {
namespace udp {

using namespace nx::hpm;

TunnelConnector::TunnelConnector(
    AddressEntry targetHostAddress,
    boost::optional<SocketAddress> mediatorAddress)
:
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(QnUuid::createUuid().toByteArray()),
    m_mediatorUdpClient(
        std::make_unique<api::MediatorClientUdpConnection>(
            mediatorAddress
            ? mediatorAddress.get()
            : *nx::network::SocketGlobals::mediatorConnector().mediatorAddress())),   //TunnelConnector MUST not be created if mediator address is unknown
    m_done(false)
{
    NX_ASSERT(nx::network::SocketGlobals::mediatorConnector().mediatorAddress());

    m_mediatorUdpClient->socket()->bindToAioThread(m_timer.getAioThread());
}

TunnelConnector::~TunnelConnector()
{
    //it is OK if called after pleaseStop or within aio thread after connect handler has been called
}

void TunnelConnector::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_timer.post(
        [this, handler = std::move(handler)]() mutable
        {
            //m_udtConnection cannot be modified since all processing is already stopped
            m_mediatorUdpClient.reset();
            m_timer.pleaseStopSync();
            m_rendezvousConnectors.clear();
            m_udtConnection.reset();    //we do not use this connection so can just remove it here
            m_connectResultReportSender.reset();
            handler();
        });
}

aio::AbstractAioThread* TunnelConnector::getAioThread() const
{
    return m_timer.getAioThread();
}

void TunnelConnector::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_timer.bindToAioThread(aioThread);
    m_mediatorUdpClient->socket()->bindToAioThread(aioThread);
}

void TunnelConnector::post(nx::utils::MoveOnlyFunc<void()> func)
{
    m_timer.post(std::move(func));
}

void TunnelConnector::dispatch(nx::utils::MoveOnlyFunc<void()> func)
{
    m_timer.dispatch(std::move(func));
}

int TunnelConnector::getPriority() const
{
    //TODO #ak
    return 0;
}

void TunnelConnector::connect(
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection>)> handler)
{
    if (!m_mediatorUdpClient->socket()->setReuseAddrFlag(true) ||
        !m_mediatorUdpClient->socket()->bind(
            SocketAddress(HostAddress::anyHost, 0)))
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("cross-nat %1. Failed to bind to mediator udp client to local port. %2")
            .arg(m_connectSessionId).arg(SystemError::getLastOSErrorText()),
            cl_logWARNING);
        m_timer.post(
            [handler = move(handler), errorCode]() mutable
            {
                handler(errorCode, nullptr);
            });
        return;
    }

    m_localAddress = m_mediatorUdpClient->socket()->getLocalAddress();

    NX_LOGX(lm("cross-nat %1. connecting to %2 with timeout %3, from local port %4")
        .arg(m_connectSessionId).arg(m_targetHostAddress.host.toString())
        .arg(timeout).arg(m_mediatorUdpClient->socket()->getLocalAddress().port),
        cl_logDEBUG2);

    if (timeout > std::chrono::milliseconds::zero())
    {
        m_connectTimeout = timeout;
        m_timer.start(
            timeout,
            std::bind(&TunnelConnector::onTimeout, this));
    }

    m_completionHandler = std::move(handler);

    m_connectResultReport.resultCode =
        api::UdpHolePunchingResultCode::noResponseFromMediator;

    api::ConnectRequest connectRequest;
    connectRequest.originatingPeerID = QnUuid::createUuid().toByteArray();
    connectRequest.connectSessionId = m_connectSessionId;
    connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
    connectRequest.destinationHostName = m_targetHostAddress.host.toString().toUtf8();
    if (m_originatingHostAddressReplacement)
    {
        connectRequest.ignoreSourceAddress = true;
        connectRequest.udpEndpointList.emplace_back(
            SocketAddress(*m_originatingHostAddressReplacement, 0));    //in case of zero port mediator will take request source port
    }
    using namespace std::placeholders;
    m_mediatorUdpClient->connect(
        std::move(connectRequest),
        std::bind(&TunnelConnector::onConnectResponse, this, _1, _2));
}

const AddressEntry& TunnelConnector::targetPeerAddress() const
{
    return m_targetHostAddress;
}

SocketAddress TunnelConnector::localAddress() const
{
    return m_localAddress;
}

void TunnelConnector::replaceOriginatingHostAddress(const QString& hostAddress)
{
    m_originatingHostAddressReplacement = hostAddress;
}

void TunnelConnector::messageReceived(
    SocketAddress sourceAddress,
    stun::Message msg)
{
    //here we can receive response to connect result report. We just don't need it
}

void TunnelConnector::ioFailure(SystemError::ErrorCode /*errorCode*/)
{
    //if error happens when sending connect result report, 
    //  it will be reported to TunnelConnector::connectSessionReportSent too
    //  and we will handle error there
}

namespace {
api::UdpHolePunchingResultCode mediatorResultToHolePunchingResult(
    api::ResultCode resultCode)
{
    switch (resultCode)
    {
        case api::ResultCode::ok:
            return api::UdpHolePunchingResultCode::ok;
        case api::ResultCode::networkError:
        case api::ResultCode::timedOut:
            return api::UdpHolePunchingResultCode::noResponseFromMediator;
        default:
            return api::UdpHolePunchingResultCode::mediatorReportedError;
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
}

void TunnelConnector::onConnectResponse(
    api::ResultCode resultCode,
    api::ConnectResponse response)
{
    if (m_done)
        return;

    //if failed, reporting error
    if (resultCode != api::ResultCode::ok)
    {
        NX_LOGX(lm("cross-nat %1. mediator reported %2 error code on connect request to %3")
            .arg(m_connectSessionId).arg(QnLexical::serialized(resultCode))
            .arg(m_targetHostAddress.host.toString().toUtf8()),
            cl_logDEBUG1);
        return holePunchingDone(
            mediatorResultToHolePunchingResult(resultCode),
            mediatorResultToSysErrorCode(resultCode));
    }

    m_connectResultReport.resultCode =
        api::UdpHolePunchingResultCode::targetPeerHasNoUdpAddress;

    //extracting target address from response
    if (response.udpEndpointList.empty())
    {
        NX_LOGX(lm("cross-nat %1. mediator reported empty UDP address list for host %2")
            .arg(m_connectSessionId).arg(m_targetHostAddress.host.toString()),
            cl_logDEBUG1);
        return holePunchingDone(
            api::UdpHolePunchingResultCode::targetPeerHasNoUdpAddress,
            SystemError::connectionReset);
    }
    //TODO #ak what if there are several addresses in udpEndpointList?

    m_connectResultReport.resultCode =
        api::UdpHolePunchingResultCode::noSynFromTargetPeer;

    //initiating rendezvous connect to the target host
    m_timer.cancelSync();   //we are in timer's thread
    //from now on relying on UdtConnection timeout
    using namespace std::chrono;
    milliseconds effectiveConnectTimeout(0);
    if (m_connectTimeout)
    {
        effectiveConnectTimeout = duration_cast<milliseconds>(m_timer.timeToEvent());
        if (effectiveConnectTimeout == milliseconds::zero())
            effectiveConnectTimeout = milliseconds(1);   //zero timeout is infinity
    }

    auto rendezvousConnector = std::make_unique</*RendezvousConnector*/RendezvousConnectorWithVerification>(
        m_connectSessionId,
        std::move(response.udpEndpointList.front()),
        std::move(m_mediatorUdpClient->takeSocket())); //moving system socket handler from m_mediatorUdpClient to udt connection
    rendezvousConnector->bindToAioThread(m_timer.getAioThread());

    m_mediatorUdpClient.reset();
    NX_LOGX(lm("cross-nat %1. Udt rendezvous connect to %2")
        .arg(m_connectSessionId).arg(rendezvousConnector->remoteAddress().toString()),
        cl_logDEBUG1);

    rendezvousConnector->connect(
        effectiveConnectTimeout,
        [this, rendezvousConnectorPtr = rendezvousConnector.get()](
            SystemError::ErrorCode errorCode,
            std::unique_ptr<UdtStreamSocket> udtConnection)
        {
            onUdtConnectionEstablished(
                rendezvousConnectorPtr,
                std::move(udtConnection),
                errorCode);
        });
    m_rendezvousConnectors.emplace_back(std::move(rendezvousConnector));
}

void TunnelConnector::onUdtConnectionEstablished(
    RendezvousConnector* rendezvousConnectorPtr,
    std::unique_ptr<UdtStreamSocket> udtConnection,
    SystemError::ErrorCode errorCode)
{
    //we are in m_timer's aio thread
    if (m_done)
        return; //just ignoring

    auto rendezvousConnectorIter = std::find_if(
        m_rendezvousConnectors.begin(),
        m_rendezvousConnectors.end(),
        [rendezvousConnectorPtr](
            const std::unique_ptr<RendezvousConnector>& val)
        {
            return val.get() == rendezvousConnectorPtr;
        });
    NX_ASSERT(rendezvousConnectorIter != m_rendezvousConnectors.end());
    auto rendezvousConnector = std::move(*rendezvousConnectorIter);
    m_rendezvousConnectors.erase(rendezvousConnectorIter);

    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("cross-nat %1. Failed to establish UDT cross-nat to %2. %3")
            .arg(m_connectSessionId)
            .arg(rendezvousConnector->remoteAddress().toString())
            .arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);

        if (!m_rendezvousConnectors.empty())
            return; //waiting for other connectors to complete
        holePunchingDone(
            api::UdpHolePunchingResultCode::udtConnectFailed,
            errorCode);
        return;
    }
    
    //success!
    NX_LOGX(lm("cross-nat %1. Udp hole punching to %2 is a success!")
        .arg(m_connectSessionId).arg(rendezvousConnector->remoteAddress().toString()),
        cl_logDEBUG2);

    //stopping other rendezvous connectors
    m_rendezvousConnectors.clear(); //can do since we are in aio thread

    NX_CRITICAL(udtConnection);
    m_udtConnection = std::move(udtConnection);
    rendezvousConnector.reset();

    //introducing delay to give server some time to call accept (work around udt bug)
    m_timer.cancelSync();
    m_timer.start(
        std::chrono::milliseconds(200),
        [this]
        {
            holePunchingDone(
                api::UdpHolePunchingResultCode::ok,
                SystemError::noError);
        });
}

void TunnelConnector::onTimeout()
{
    NX_LOGX(lm("cross-nat %1 timed out. Result code %2")
        .arg(QnLexical::serialized(m_connectResultReport.resultCode)),
        cl_logDEBUG1);

    NX_ASSERT(m_udtConnection == nullptr);
    m_rendezvousConnectors.clear(); //can do since we are in aio thread

    holePunchingDone(
        m_connectResultReport.resultCode,
        SystemError::timedOut);
}

void TunnelConnector::holePunchingDone(
    api::UdpHolePunchingResultCode resultCode,
    SystemError::ErrorCode sysErrorCode)
{
    NX_LOGX(lm("cross-nat %1. result: %2, system result code: %3")
        .arg(m_connectSessionId).arg(QnLexical::serialized(resultCode))
        .arg(SystemError::toString(sysErrorCode)),
        cl_logDEBUG2);

    //we are in aio thread
    m_timer.cancelSync();

    //stopping processing of incoming packets, events...
    m_done = true;  //TODO #ak redundant?

    m_connectResultReport.sysErrorCode = sysErrorCode;
    if (resultCode == api::UdpHolePunchingResultCode::noResponseFromMediator)
    {
        //not sending report to mediator since no answer from mediator...
        return connectSessionReportSent(SystemError::noError);
    }

    //reporting result to mediator
        //after message has been sent - reporting result to client
    m_connectResultReport.connectSessionId = m_connectSessionId;
    m_connectResultReport.resultCode = resultCode;

    using namespace std::placeholders;
    m_connectResultReportSender =
        std::make_unique<stun::UnreliableMessagePipeline>(this);
    m_connectResultReportSender->bindToAioThread(m_timer.getAioThread());
    stun::Message connectResultReportMessage(
        stun::Header(
            stun::MessageClass::request,
            stun::cc::methods::connectionResult));
    m_connectResultReport.serialize(&connectResultReportMessage);
    m_connectResultReportSender->sendMessage(
        *SocketGlobals::mediatorConnector().mediatorAddress(),
        std::move(connectResultReportMessage),
        std::bind(&TunnelConnector::connectSessionReportSent, this, _1));
}

void TunnelConnector::connectSessionReportSent(
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
    std::unique_ptr<AbstractOutgoingTunnelConnection> tunnelConnection;
    if (m_connectResultReport.resultCode != api::UdpHolePunchingResultCode::ok)
    {
        sysErrorCodeToReport =
            m_connectResultReport.sysErrorCode == SystemError::noError
            ? SystemError::connectionReset
            : m_connectResultReport.sysErrorCode;
    }
    else
    {
        tunnelConnection = std::make_unique<OutgoingTunnelConnection>(
            m_connectSessionId,
            std::move(m_udtConnection));
    }
    auto completionHandler = std::move(m_completionHandler);
    NX_LOGX(lm("cross-nat %1. report send result code: %2. Invoking handler with result: %3")
        .arg(m_connectSessionId).arg(SystemError::toString(errorCode))
        .arg(SystemError::toString(sysErrorCodeToReport)),
        cl_logDEBUG2);
    completionHandler(sysErrorCodeToReport, std::move(tunnelConnection));
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
