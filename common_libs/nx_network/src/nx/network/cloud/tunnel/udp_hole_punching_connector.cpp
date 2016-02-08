/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_connector.h"


namespace nx {
namespace network {
namespace cloud {

UdpHolePunchingTunnelConnector::UdpHolePunchingTunnelConnector(
    AddressEntry targetHostAddress)
:
    m_targetHostAddress(std::move(targetHostAddress))
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
    //TODO #ak
}
    
const AddressEntry& UdpHolePunchingTunnelConnector::targetPeerAddress() const
{
    return m_targetHostAddress;
}

} // namespace cloud
} // namespace network
} // namespace nx
