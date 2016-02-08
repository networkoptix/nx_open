/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "abstract_tunnel_connector.h"


namespace nx {
namespace network {
namespace cloud {

class UdpHolePunchingTunnelConnector
:
    public AbstractTunnelConnector
{
public:
    UdpHolePunchingTunnelConnector(AddressEntry targetHostAddress);

    virtual void pleaseStop(std::function<void()> handler) override;

    virtual int getPriority() const override;
    virtual void connect(
        std::chrono::milliseconds timeout,
        std::function<void(
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractTunnelConnection>)> handler) override;
    virtual const AddressEntry& targetPeerAddress() const override;

private:
    const AddressEntry m_targetHostAddress;
};

} // namespace cloud
} // namespace network
} // namespace nx
