/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "abstract_tunnel_connector.h"

#include "nx/network/cloud/data/connect_data.h"
#include "nx/network/cloud/mediator_connections.h"


namespace nx {
namespace network {
namespace cloud {

/** Establishes cross-nat connection to the specified host using UDP hole punching technique.
    \note One instance can keep only one session
 */
class UdpHolePunchingTunnelConnector
:
    public AbstractTunnelConnector
{
public:
    UdpHolePunchingTunnelConnector(AddressEntry targetHostAddress);

    virtual void pleaseStop(std::function<void()> handler) override;

    virtual int getPriority() const override;
    /** Only one connect can be running at a time. */
    virtual void connect(
        std::chrono::milliseconds timeout,
        std::function<void(
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractTunnelConnection>)> handler) override;
    virtual const AddressEntry& targetPeerAddress() const override;

private:
    const AddressEntry m_targetHostAddress;
    const nx::String m_connectSessionID;
    nx::hpm::api::MediatorClientUdpConnection m_mediatorUdpClient;
    std::function<void(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractTunnelConnection>)> m_completionHandler;

    void onConnectResponse(
        nx::hpm::api::ResultCode resultCode,
        nx::hpm::api::ConnectResponse response);
    void holePunchingDone();
};

} // namespace cloud
} // namespace network
} // namespace nx
