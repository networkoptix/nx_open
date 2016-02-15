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
    AddressEntry targetHostAddress)
:
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(QnUuid::createUuid().toByteArray()),
    m_mediatorUdpClient(
        new api::MediatorClientUdpConnection(
            *nx::network::SocketGlobals::mediatorConnector().mediatorAddress())),   //UdpHolePunchingTunnelConnector MUST not be created if mediator address is unknown
    m_udpPipeline(new stun::UnreliableMessagePipeline(this)),
    m_done(false)
{
    assert(nx::network::SocketGlobals::mediatorConnector().mediatorAddress());

    m_mediatorUdpClient->socket()->bindToAioThread(m_timer.getAioThread());
    m_udpPipeline->socket()->bindToAioThread(m_timer.getAioThread());
}

UdpHolePunchingTunnelConnector::~UdpHolePunchingTunnelConnector()
{
    //it is OK if called after pleaseStop or within aio thread after connect handler has been called
    m_mediatorUdpClient.reset();
    m_udpPipeline.reset();
    m_udtConnection.reset();
}

void UdpHolePunchingTunnelConnector::pleaseStop(std::function<void()> handler)
{
    m_timer.post(
        [this, handler = std::move(handler)]() mutable
        {
            //m_udtConnection cannot be modified since all processing is already stopped
            m_mediatorUdpClient.reset();
            m_udpPipeline.reset();
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
    std::function<void(
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

    if (!m_udpPipeline->socket()->setReuseAddrFlag(true) ||
        !m_udpPipeline->socket()->bind(m_mediatorUdpClient->localAddress()))
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("session %1. Failed to share local UDP address %2. %3")
            .arg(m_connectSessionId)
            .arg(m_mediatorUdpClient->localAddress().toString())
            .arg(SystemError::getLastOSErrorText()),
            cl_logWARNING);
        m_timer.post(
            [handler = move(handler), errorCode]() mutable
            {
                handler(errorCode, nullptr);
            });
        return;
    }

    m_timer.start(
        timeout,
        std::bind(&UdpHolePunchingTunnelConnector::onTimeout, this));

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

void UdpHolePunchingTunnelConnector::messageReceived(
    SocketAddress msgSourceAddress,
    stun::Message message)
{
    if (m_done)
        return;
    //here we expect SYN request from the target peer

    if (!m_targetHostUdpAddress ||
        msgSourceAddress != *m_targetHostUdpAddress ||
        message.header.method != stun::cc::methods::udpHolePunchingSyn)
    {
        //NOTE in some cases we can receive SYN request before response from mediator. 
        //Currently, we just ignore this case. Maybe, we can fill m_targetHostUdpAddress 
        //if message.attribute<connectSessionId> match m_connectSessionId
        NX_LOGX(lm("session %1. Received unexpected STUN %2 message from %3")
            .arg(m_connectSessionId).arg(message.header.method),
            cl_logDEBUG1);
        return;
    }

    api::UdpHolePunchingSyn synRequest;
    if (!synRequest.parse(message))
    {
        NX_LOGX(lm("session %1. Failed to parse STUN message from %2")
            .arg(m_connectSessionId).arg(msgSourceAddress.toString()),
            cl_logDEBUG1);
        return;
    }

    if (synRequest.connectSessionId != m_connectSessionId)
    {
        NX_LOGX(lm("session %1. Received SYN with unknown connect session id (%2) from %3")
            .arg(m_connectSessionId).arg(synRequest.connectSessionId)
            .arg(msgSourceAddress.toString()),
            cl_logDEBUG1);
        return;
    }

    NX_LOGX(lm("session %1. Received SYN request message from %2")
        .arg(m_connectSessionId).arg(msgSourceAddress.toString()),
        cl_logDEBUG2);

    //sending SYN-ACK in response
    api::UdpHolePunchingSynAck synAckData;
    synAckData.connectSessionId = m_connectSessionId;
    stun::Message synAckMessage(
        nx::stun::Header(
            stun::MessageClass::successResponse,
            stun::cc::methods::udpHolePunchingSynAck));
    synAckData.serialize(&synAckMessage);
    m_udpPipeline->sendMessage(
        msgSourceAddress,
        std::move(synAckMessage),
        [](SystemError::ErrorCode /*errorCode*/, SocketAddress) {}); //ignoring send error, since we will failed with timeout anyway
}

void UdpHolePunchingTunnelConnector::ioFailure(
    SystemError::ErrorCode errorCode)
{
    NX_LOGX(lm("session %1. Udp pipeline reported error. %2")
        .arg(SystemError::toString(errorCode)), cl_logDEBUG1);
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
            api::UdpHolePunchingResultCode::noResponseFromMediator,
            SystemError::connectionReset);
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

    //initiating hole punching
    //sending SYN to the target host
    api::UdpHolePunchingSyn requestData;
    requestData.connectSessionId = m_connectSessionId;
    stun::Message synRequest(
        nx::stun::Header(
            stun::MessageClass::request,
            stun::cc::methods::udpHolePunchingSyn));
    requestData.serialize(&synRequest);

    using namespace std::placeholders;
    //TODO #ak here I use m_mediatorUdpClient to send request not to mediator, 
        //but to the target peer. 
        //Maybe, it is better to have another udp client?
    m_mediatorUdpClient->sendRequestTo(
        *m_targetHostUdpAddress,
        std::move(synRequest),
        /* custom retransmission parameters, */
        std::bind(&UdpHolePunchingTunnelConnector::onSynAckReceived, this, _1, _2));
}

void UdpHolePunchingTunnelConnector::onSynAckReceived(
    SystemError::ErrorCode errorCode,
    stun::Message synAckMessage)
{
    if (m_done)
        return;

    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("session %1. No response from target host. %2")
            .arg(m_connectSessionId).arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);
        return holePunchingDone(
            api::UdpHolePunchingResultCode::noSynFromTargetPeer,
            errorCode);
    }

    //received response from target host. Looks like, hole punching works...
    //initiating UDT connection...
    auto udtClientSocket = std::make_unique<UdtStreamSocket>();
    //udtClientSocket->bindToAioThread(m_mediatorUdpClient->socket()->getAioThread()); //TODO #ak udt sockets live in another thread pool!
    if (!udtClientSocket->setReuseAddrFlag(true) ||
        !udtClientSocket->bind(m_mediatorUdpClient->localAddress()))
    {
        const auto sysErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("session %1. Failed to bind UDT socket to local address %2. %3")
            .arg(m_connectSessionId)
            .arg(m_mediatorUdpClient->localAddress().toString())
            .arg(SystemError::toString(sysErrorCode)),
            cl_logDEBUG1);
        return holePunchingDone(
            api::UdpHolePunchingResultCode::udtConnectFailed,
            errorCode);
    }

    m_connectResultReport.resultCode =
        api::UdpHolePunchingResultCode::udtConnectFailed;

    m_udtConnection = std::move(udtClientSocket);
    assert(m_targetHostUdpAddress);
    m_udtConnection->connectAsync(
        *m_targetHostUdpAddress,
        [this](SystemError::ErrorCode errorCode)
        {
            //TODO #ak currently, regular and UDT sockets live in different aio threads.
                //That's why we have to use all that posts here 
                //to move execution to m_timer's aio thread
            m_udtConnection->post(
                [this, errorCode]()
                {
                    //m_udtConnection can be safely removed by now
                    //moving execution m_timer's aio thread
                    m_timer.post(std::bind(
                        &UdpHolePunchingTunnelConnector::onUdtConnectionEstablished,
                        this,
                        errorCode));
                });
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

    holePunchingDone(
        api::UdpHolePunchingResultCode::ok,
        SystemError::noError);
}

void UdpHolePunchingTunnelConnector::onTimeout()
{
    NX_LOGX(lm("session %1 timed out. Result code %2")
        .arg(QnLexical::serialized(m_connectResultReport.resultCode)),
        cl_logDEBUG1);

    if (!m_udtConnection)
    {
        holePunchingDone(
            m_connectResultReport.resultCode,
            SystemError::timedOut);
        return;
    }

    //stopping UDT connection
    m_udtConnection->post(
        [this]()
        {
            //cancelling connect
            m_udtConnection->cancelIOSync(aio::etWrite);

            //TODO #ak remove this embedded post after switching to 
                //the single aio thread pool
            m_timer.post(
                [this]()
                {
                    if (m_done)
                        return;
                    holePunchingDone(
                        m_connectResultReport.resultCode,
                        SystemError::timedOut);
                });
        });
}

void UdpHolePunchingTunnelConnector::holePunchingDone(
    api::UdpHolePunchingResultCode resultCode,
    SystemError::ErrorCode sysErrorCode)
{
    //we are in aio thread
    m_timer.cancelSync();

    //stopping processing of incoming packets, events...
    m_done = true;

    if (resultCode == api::UdpHolePunchingResultCode::noResponseFromMediator)
    {
        //not sending report to mediator since no answer from mediator...
        return connectSessionReportSent(SystemError::noError);
    }

    //reporting result to mediator
        //after message has been sent - reporting result to client
    m_connectResultReport.connectSessionId = m_connectSessionId;
    m_connectResultReport.resultCode = resultCode;
    m_connectResultReport.sysErrorCode = sysErrorCode;
    using namespace std::placeholders;
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
            std::move(m_udtConnection),
            *m_targetHostUdpAddress);
    }
    auto completionHandler = std::move(m_completionHandler);
    completionHandler(sysErrorCodeToReport, nullptr);
}

} // namespace cloud
} // namespace network
} // namespace nx
