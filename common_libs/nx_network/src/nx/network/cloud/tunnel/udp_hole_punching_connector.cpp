/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_connector.h"

#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>

#include <utils/serialization/lexical.h>


namespace nx {
namespace network {
namespace cloud {

using namespace nx::hpm;

//TODO #ak this is a temporary address
const char* mediatorEndpoint = "cloud-demo.hdw.mx:3345";

UdpHolePunchingTunnelConnector::UdpHolePunchingTunnelConnector(
    AddressEntry targetHostAddress)
:
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(QnUuid::createUuid().toByteArray()),
    m_mediatorUdpClient(mediatorEndpoint),
    m_udpPipeline(this)
{
    m_udpPipeline.socket()->bindToAioThread(m_mediatorUdpClient.socket()->getAioThread());
}

void UdpHolePunchingTunnelConnector::pleaseStop(std::function<void()> handler)
{
    //TODO #ak
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
        std::unique_ptr<AbstractTunnelConnection>)> handler)
{
    NX_LOGX(lm("session %1. connecting to %2 with timeout %3 ms").
        arg(m_connectSessionId).
        arg(m_targetHostAddress.host.toString()).arg(timeout.count()),
        cl_logDEBUG2);

    if (!m_mediatorUdpClient.socket()->setReuseAddrFlag(true) ||
        !m_mediatorUdpClient.socket()->bind(SocketAddress(HostAddress::anyHost, 0)))
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("session %1. Failed to bind to mediator udp client to local port. %2").
            arg(m_connectSessionId).arg(SystemError::getLastOSErrorText()),
            cl_logWARNING);
        m_mediatorUdpClient.socket()->post(
            [handler = move(handler), errorCode]() mutable
            {
                handler(errorCode, nullptr);
            });
        return;
    }

    if (!m_udpPipeline.socket()->setReuseAddrFlag(true) ||
        !m_udpPipeline.socket()->bind(m_mediatorUdpClient.localAddress()))
    {
        const auto errorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("session %1. Failed to share mediator udp client's local address %2. %3").
            arg(m_connectSessionId).arg(m_mediatorUdpClient.localAddress().toString()).
            arg(SystemError::getLastOSErrorText()),
            cl_logWARNING);
        m_mediatorUdpClient.socket()->post(
            [handler = move(handler), errorCode]() mutable
            {
                handler(errorCode, nullptr);
            });
        return;
    }

    m_completionHandler = std::move(handler);

    api::ConnectRequest connectRequest;
    connectRequest.originatingPeerID = QnUuid::createUuid().toByteArray();
    connectRequest.connectSessionId = m_connectSessionId;
    connectRequest.connectionMethods = api::ConnectionMethod::udpHolePunching;
    connectRequest.destinationHostName = m_targetHostAddress.host.toString().toUtf8();
    using namespace std::placeholders;
    m_mediatorUdpClient.connect(
        connectRequest,
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
    if (!m_targetHostUdpAddress ||
        msgSourceAddress != *m_targetHostUdpAddress ||
        message.header.method != stun::cc::methods::udpHolePunchingSyn)
    {
        //NOTE in some cases we can receive SYN request before response from mediator. 
        //Currently, we just ignore this case. Maybe, we can fill m_targetHostUdpAddress 
        //if message.attribute<connectSessionId> match m_connectSessionId
        NX_LOGX(lm("session %1. Received unexpected STUN %2 message from %3").
            arg(m_connectSessionId).arg(message.header.method),
            cl_logDEBUG1);
        return;
    }

    api::UdpHolePunchingSyn synRequest;
    if (!synRequest.parse(message))
    {
        NX_LOGX(lm("session %1. Failed to parse STUN message from %2").
            arg(m_connectSessionId).arg(msgSourceAddress.toString()),
            cl_logDEBUG1);
        return;
    }

    if (synRequest.connectSessionId != m_connectSessionId)
    {
        NX_LOGX(lm("session %1. Received SYN with unknown connect session id (%2) from %3").
            arg(m_connectSessionId).arg(synRequest.connectSessionId).
            arg(msgSourceAddress.toString()),
            cl_logDEBUG1);
        return;
    }

    NX_LOGX(lm("session %1. Received SYN request message from %2").
        arg(m_connectSessionId).arg(msgSourceAddress.toString()),
        cl_logDEBUG2);

    //sending SYN-ACK in response
    api::UdpHolePunchingSynAck synAckData;
    synAckData.connectSessionId = m_connectSessionId;
    stun::Message synAckMessage(
        nx::stun::Header(
            stun::MessageClass::request,
            stun::cc::methods::udpHolePunchingSynAck));
    synAckData.serialize(&synAckMessage);
    m_udpPipeline.sendMessage(
        msgSourceAddress,
        std::move(synAckMessage),
        [](SystemError::ErrorCode /*errorCode*/, SocketAddress) {}); //TODO #ak handle send error
}

void UdpHolePunchingTunnelConnector::ioFailure(SystemError::ErrorCode /*errorCode*/)
{
    //TODO #ak
}

void UdpHolePunchingTunnelConnector::onConnectResponse(
    api::ResultCode resultCode,
    api::ConnectResponse response)
{
    //if failed, reporting error
    if (resultCode != api::ResultCode::ok)
    {
        NX_LOGX(lm("session %1. mediator reported %2 error code on connect request to %3").
            arg(m_connectSessionId).
            arg(QnLexical::serialized(resultCode)).
            arg(m_targetHostAddress.host.toString().toUtf8()),
            cl_logDEBUG1);
        auto completionHandler = std::move(m_completionHandler);
        return completionHandler(SystemError::connectionReset, nullptr);
    }

    //extracting m_targetHostUdpAddress from response
    if (response.udpEndpointList.empty())
    {
        NX_LOGX(lm("session %1. mediator reported empty UDP address list for host %2").
            arg(m_connectSessionId).arg(m_targetHostAddress.host.toString()),
            cl_logDEBUG1);
        auto completionHandler = std::move(m_completionHandler);
        return completionHandler(SystemError::connectionReset, nullptr);
    }
    //TODO #ak what if there are several addresses in udpEndpointList?
    m_targetHostUdpAddress = std::move(response.udpEndpointList.front());

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
    m_mediatorUdpClient.sendRequestTo(
        *m_targetHostUdpAddress,
        std::move(synRequest),
        /* custom retransmission parameters, */
        std::bind(&UdpHolePunchingTunnelConnector::onSynAckReceived, this, _1, _2));
}

void UdpHolePunchingTunnelConnector::onSynAckReceived(
    SystemError::ErrorCode errorCode,
    stun::Message synAckMessage)
{
    //TODO #ak ignore retransmit

    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("session %1. No response from target host. %2").
            arg(m_connectSessionId).arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);
        return holePunchingDone();
    }

    //received response from target host. Looks like, hole punching works...
    //initiating UDT connection...
    auto udtClientSocket = std::make_unique<UdtStreamSocket>();
    //udtClientSocket->bindToAioThread(m_mediatorUdpClient.socket()->getAioThread()); //TODO #ak udt sockets live in another thread pool!
    if (!udtClientSocket->setReuseAddrFlag(true) ||
        !udtClientSocket->bind(m_mediatorUdpClient.localAddress()))
    {
        const auto sysErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("session %1. Failed to bind UDT socket to local address %2. %3").
            arg(m_connectSessionId).arg(m_mediatorUdpClient.localAddress().toString()).
            arg(SystemError::toString(sysErrorCode)), cl_logDEBUG1);
        return holePunchingDone();
    }

    m_udtConnection = std::move(udtClientSocket);
    assert(m_targetHostUdpAddress);
    using namespace std::placeholders;
    m_udtConnection->connectAsync(
        *m_targetHostUdpAddress,
         std::bind(&UdpHolePunchingTunnelConnector::onUdtConnectionEstablished, this, _1));
}

void UdpHolePunchingTunnelConnector::onUdtConnectionEstablished(
    SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("session %1. Failed to establish UDT connection to %2. %3").
            arg(m_connectSessionId).arg(m_targetHostUdpAddress->toString()).
            arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);
        m_udtConnection.reset();
        return holePunchingDone();
    }
    
    //success!
    NX_LOGX(lm("session %2. Udp hole punching is a success!"), cl_logDEBUG2);
    
    m_udpPipeline.socket()->post(
        std::bind(&UdpHolePunchingTunnelConnector::holePunchingDone, this));
}

void UdpHolePunchingTunnelConnector::holePunchingDone()
{
    //TODO #ak reporting result to mediator
        //after message has been sent - reporting result to client

    api::ConnectionResultRequest resultData;
    resultData.connectSessionId = m_connectSessionId;
    using namespace std::placeholders;
    m_mediatorUdpClient.connectionResult(
        std::move(resultData),
        [](api::ResultCode){ /* ignoring result */ });
    //currently, m_mediatorUdpClient does not provide a way to receive request send result
        //so, just giving it a chance to send report
        //TODO #ak send it reliably
    m_mediatorUdpClient.socket()->post(std::bind(
        &UdpHolePunchingTunnelConnector::connectSessionReportSent,
        this,
        SystemError::noError));
}

void UdpHolePunchingTunnelConnector::connectSessionReportSent(
    SystemError::ErrorCode /*errorCode*/)
{
    auto completionHandler = std::move(m_completionHandler);
    completionHandler(SystemError::connectionReset, nullptr);
}

} // namespace cloud
} // namespace network
} // namespace nx
