/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_connector.h"

#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>

#include <utils/serialization/lexical.h>

#include "outgoing_tunnel_udt_connection.h"


namespace nx {
namespace network {
namespace cloud {

using namespace nx::hpm;

UdpHolePunchingTunnelConnector::UdpHolePunchingTunnelConnector(
    AddressEntry targetHostAddress,
    boost::optional<SocketAddress> mediatorAddress)
:
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(QnUuid::createUuid().toByteArray()),
    m_mediatorUdpClient(
        std::make_unique<api::MediatorClientUdpConnection>(
            mediatorAddress
            ? mediatorAddress.get()
            : *nx::network::SocketGlobals::mediatorConnector().mediatorAddress())),   //UdpHolePunchingTunnelConnector MUST not be created if mediator address is unknown
    m_done(false)
{
    NX_ASSERT(nx::network::SocketGlobals::mediatorConnector().mediatorAddress());

    m_mediatorUdpClient->socket()->bindToAioThread(m_timer.getAioThread());
}

UdpHolePunchingTunnelConnector::~UdpHolePunchingTunnelConnector()
{
    //it is OK if called after pleaseStop or within aio thread after connect handler has been called
    m_mediatorUdpClient.reset();
    m_udtConnection.reset();
}

void UdpHolePunchingTunnelConnector::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_timer.post(
        [this, handler = std::move(handler)]() mutable
        {
            //m_udtConnection cannot be modified since all processing is already stopped
            m_mediatorUdpClient.reset();
            m_timer.pleaseStopSync();
            if (!m_udtConnection)
                return handler();
            m_udtConnection->pleaseStop(std::move(handler));
        });
}

int UdpHolePunchingTunnelConnector::getPriority() const
{
    //TODO #ak
    return 0;
}

void UdpHolePunchingTunnelConnector::connect(
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection>)> handler)
{
    NX_LOGX(lm("session %1. connecting to %2 with timeout %3")
        .arg(m_connectSessionId).arg(m_targetHostAddress.host.toString())
        .arg(timeout),
        cl_logDEBUG2);

    if (!m_mediatorUdpClient->socket()->setReuseAddrFlag(true) ||
        !m_mediatorUdpClient->socket()->bind(
            SocketAddress(HostAddress::anyHost, 0)))
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("session %1. Failed to bind to mediator udp client to local port. %2")
            .arg(m_connectSessionId).arg(SystemError::getLastOSErrorText()),
            cl_logWARNING);
        m_timer.post(
            [handler = move(handler), errorCode]() mutable
            {
                handler(errorCode, nullptr);
            });
        return;
    }

    if (timeout > std::chrono::milliseconds::zero())
    {
        m_connectTimeout = timeout;
        m_timer.start(
            timeout,
            std::bind(&UdpHolePunchingTunnelConnector::onTimeout, this));
    }

    m_completionHandler = std::move(handler);

    m_connectResultReport.resultCode =
        api::UdpHolePunchingResultCode::noResponseFromMediator;

    api::ConnectRequest connectRequest;
    connectRequest.originatingPeerID = QnUuid::createUuid().toByteArray();
    connectRequest.connectSessionId = m_connectSessionId;
    connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
    connectRequest.destinationHostName = m_targetHostAddress.host.toString().toUtf8();
    using namespace std::placeholders;
    m_mediatorUdpClient->connect(
        std::move(connectRequest),
        std::bind(&UdpHolePunchingTunnelConnector::onConnectResponse, this, _1, _2));
}

const AddressEntry& UdpHolePunchingTunnelConnector::targetPeerAddress() const
{
    return m_targetHostAddress;
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

void UdpHolePunchingTunnelConnector::onConnectResponse(
    api::ResultCode resultCode,
    api::ConnectResponse response)
{
    if (m_done)
        return;

    //if failed, reporting error
    if (resultCode != api::ResultCode::ok)
    {
        NX_LOGX(lm("session %1. mediator reported %2 error code on connect request to %3")
            .arg(m_connectSessionId).arg(QnLexical::serialized(resultCode))
            .arg(m_targetHostAddress.host.toString().toUtf8()),
            cl_logDEBUG1);
        return holePunchingDone(
            mediatorResultToHolePunchingResult(resultCode),
            mediatorResultToSysErrorCode(resultCode));
    }

    m_connectResultReport.resultCode =
        api::UdpHolePunchingResultCode::targetPeerHasNoUdpAddress;

    //extracting m_targetHostUdpAddress from response
    if (response.udpEndpointList.empty())
    {
        NX_LOGX(lm("session %1. mediator reported empty UDP address list for host %2")
            .arg(m_connectSessionId).arg(m_targetHostAddress.host.toString()),
            cl_logDEBUG1);
        return holePunchingDone(
            api::UdpHolePunchingResultCode::targetPeerHasNoUdpAddress,
            SystemError::connectionReset);
    }
    //TODO #ak what if there are several addresses in udpEndpointList?
    m_targetHostUdpAddress = std::move(response.udpEndpointList.front());

    m_connectResultReport.resultCode =
        api::UdpHolePunchingResultCode::noSynFromTargetPeer;

    //initiating rendezvous connect to the target host
    auto udtConnection = std::make_unique<UdtStreamSocket>();
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
    if (!udtConnection->bindToUdpSocket(std::move(*m_mediatorUdpClient->takeSocket())) ||    //moving system socket handler from m_mediatorUdpClient to m_udtConnection
        !udtConnection->setRendezvous(true) ||
        !udtConnection->setNonBlockingMode(true) ||
        !udtConnection->setSendTimeout(effectiveConnectTimeout.count()))
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("session %1. Failed to create UDT socket. %2")
            .arg(m_connectSessionId).arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);
        return holePunchingDone(
            api::UdpHolePunchingResultCode::udtConnectFailed,
            errorCode);
    }

    m_mediatorUdpClient.reset();

    udtConnection->bindToAioThread(m_timer.getAioThread());
    m_udtConnection = std::move(udtConnection);
    
    m_udtConnection->connectAsync(
        *m_targetHostUdpAddress,
        [this](SystemError::ErrorCode errorCode)
        {
            //need post here to be sure m_udtConnection can be safely removed from any thread
            m_udtConnection->post(
                std::bind(
                    &UdpHolePunchingTunnelConnector::onUdtConnectionEstablished,
                    this,
                    errorCode));
        });
}

void UdpHolePunchingTunnelConnector::onUdtConnectionEstablished(
    SystemError::ErrorCode errorCode)
{
    //we are in m_timer's aio thread
    if (m_done)
        return; //just ignoring

    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("session %1. Failed to establish UDT connection to %2. %3")
            .arg(m_connectSessionId).arg(m_targetHostUdpAddress->toString())
            .arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);
        holePunchingDone(
            api::UdpHolePunchingResultCode::udtConnectFailed,
            errorCode);
        return;
    }
    
    //success!
    NX_LOGX(lm("session %2. Udp hole punching is a success!"), cl_logDEBUG2);

    //introducing delay to give server some time to call accept
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

void UdpHolePunchingTunnelConnector::onTimeout()
{
    NX_LOGX(lm("session %1 timed out. Result code %2")
        .arg(QnLexical::serialized(m_connectResultReport.resultCode)),
        cl_logDEBUG1);

    if (m_udtConnection)
    {
        //stopping UDT connection
        m_udtConnection->cancelIOSync(aio::etWrite);
    }

    holePunchingDone(
        m_connectResultReport.resultCode,
        SystemError::timedOut);
}

void UdpHolePunchingTunnelConnector::holePunchingDone(
    api::UdpHolePunchingResultCode resultCode,
    SystemError::ErrorCode sysErrorCode)
{
    //we are in aio thread
    m_timer.cancelSync();

    //stopping processing of incoming packets, events...
    m_done = true;

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
    if (!m_mediatorUdpClient)
    {
        //m_mediatorUdpClient has given away his socket to udt socket
        m_mediatorUdpClient = std::make_unique<api::MediatorClientUdpConnection>(
            *nx::network::SocketGlobals::mediatorConnector().mediatorAddress());
        m_mediatorUdpClient->socket()->bindToAioThread(m_timer.getAioThread());
    }
    m_mediatorUdpClient->connectionResult(
        m_connectResultReport,
        [](api::ResultCode){ /* ignoring result */ });
    //currently, m_mediatorUdpClient does not provide a way to receive request send result
        //so, just giving it a chance to send report
        //TODO #ak send it reliably. But, we should not wait for mediator reply
    m_timer.post(std::bind(
        &UdpHolePunchingTunnelConnector::connectSessionReportSent,
        this,
        SystemError::noError));
}

void UdpHolePunchingTunnelConnector::connectSessionReportSent(
    SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("session %1. Failed to send report to mediator. %2")
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
        tunnelConnection = std::make_unique<OutgoingTunnelUdtConnection>(
            m_connectSessionId,
            std::move(m_udtConnection));
    }
    auto completionHandler = std::move(m_completionHandler);
    completionHandler(sysErrorCodeToReport, std::move(tunnelConnection));
}

} // namespace cloud
} // namespace network
} // namespace nx
