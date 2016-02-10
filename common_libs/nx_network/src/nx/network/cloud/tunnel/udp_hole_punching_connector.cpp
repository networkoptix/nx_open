/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_connector.h"

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
    m_connectSessionID(QnUuid::createUuid().toByteArray()),
    m_mediatorUdpClient(mediatorEndpoint)
{
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
    NX_LOGX(lm("connect to %1 with timeout %2 ms, connect session %3").
        arg(m_targetHostAddress.host.toString()).arg(timeout.count()).
        arg(m_connectSessionID),
        cl_logDEBUG2);

    m_completionHandler = std::move(handler);

    api::ConnectRequest connectRequest;
    connectRequest.originatingPeerID = QnUuid::createUuid().toByteArray();
    connectRequest.connectSessionID = m_connectSessionID;
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

void UdpHolePunchingTunnelConnector::onConnectResponse(
    api::ResultCode resultCode,
    api::ConnectResponse response)
{
    //if failed, reporting error
    if (resultCode != api::ResultCode::ok)
    {
        NX_LOGX(lm("mediator reported %1 error code on connect request to %2").
            arg(QnLexical::serialized(resultCode)).
            arg(m_targetHostAddress.host.toString().toUtf8()),
            cl_logDEBUG1);
        return;
    }

    //if succeeded - initiating cross-nat UDT connection
}

void UdpHolePunchingTunnelConnector::holePunchingDone()
{
    //TODO #ak reporting result to mediator
}

} // namespace cloud
} // namespace network
} // namespace nx
