/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "abstract_tunnel_connector.h"

#include <functional>
#include <memory>

#include <boost/optional.hpp>

#include <nx/network/aio/timer.h>
#include <nx/network/stun/udp_client.h>
#include <nx/network/udt/udt_socket.h>

#include "nx/network/cloud/data/connect_data.h"
#include "nx/network/cloud/mediator_connections.h"


namespace nx {
namespace network {
namespace cloud {

/** Establishes cross-nat connection to the specified host using UDP hole punching technique.
    \note One instance can keep only one session
    \note Can be safely freed within connect handler. 
        Otherwise, \a UdpHolePunchingTunnelConnector::pleaseStop is required
 */
class NX_NETWORK_API UdpHolePunchingTunnelConnector
:
    public AbstractTunnelConnector
{
public:
    UdpHolePunchingTunnelConnector(AddressEntry targetHostAddress);
    virtual ~UdpHolePunchingTunnelConnector();

    virtual void pleaseStop(std::function<void()> handler) override;

    virtual int getPriority() const override;
    /** Only one connect can be running at a time. */
    virtual void connect(
        std::chrono::milliseconds timeout,
        std::function<void(
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractOutgoingTunnelConnection>)> handler) override;
    virtual const AddressEntry& targetPeerAddress() const override;

private:
    const AddressEntry m_targetHostAddress;
    const nx::String m_connectSessionId;
    std::unique_ptr<nx::hpm::api::MediatorClientUdpConnection> m_mediatorUdpClient;
    std::function<void(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection>)> m_completionHandler;
    boost::optional<SocketAddress> m_targetHostUdpAddress;
    std::unique_ptr<UdtStreamSocket> m_udtConnection;
    nx::hpm::api::ConnectionResultRequest m_connectResultReport;
    nx::network::aio::Timer m_timer;
    bool m_done;

    void onConnectResponse(
        nx::hpm::api::ResultCode resultCode,
        nx::hpm::api::ConnectResponse response);
    void onUdtConnectionEstablished(
        SystemError::ErrorCode errorCode);
    void onTimeout();
    /** always called within aio thread */
    void holePunchingDone(
        nx::hpm::api::UdpHolePunchingResultCode resultCode,
        SystemError::ErrorCode sysErrorCode);
    void connectSessionReportSent(
        SystemError::ErrorCode /*errorCode*/);
};

} // namespace cloud
} // namespace network
} // namespace nx
