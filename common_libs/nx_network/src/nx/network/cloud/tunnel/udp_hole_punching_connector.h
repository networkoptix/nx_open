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

class UdpHolePunchingRendezvousConnector;

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
    /**
        @param mediatorAddress This param is for test only
    */
    UdpHolePunchingTunnelConnector(
        AddressEntry targetHostAddress,
        boost::optional<SocketAddress> mediatorAddress = boost::none);
    virtual ~UdpHolePunchingTunnelConnector();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual aio::AbstractAioThread* getAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    virtual int getPriority() const override;
    /** Only one connect can be running at a time. */
    virtual void connect(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode errorCode,
            std::unique_ptr<AbstractOutgoingTunnelConnection>)> handler) override;
    virtual const AddressEntry& targetPeerAddress() const override;

private:
    const AddressEntry m_targetHostAddress;
    const nx::String m_connectSessionId;
    std::unique_ptr<nx::hpm::api::MediatorClientUdpConnection> m_mediatorUdpClient;
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection>)> m_completionHandler;
    std::unique_ptr<UdtStreamSocket> m_udtConnection;
    nx::hpm::api::ConnectionResultRequest m_connectResultReport;
    nx::network::aio::Timer m_timer;
    bool m_done;
    boost::optional<std::chrono::milliseconds> m_connectTimeout;
    std::deque<std::unique_ptr<UdpHolePunchingRendezvousConnector>> m_rendezvousConnectors;

    void onConnectResponse(
        nx::hpm::api::ResultCode resultCode,
        nx::hpm::api::ConnectResponse response);
    void onUdtConnectionEstablished(
        UdpHolePunchingRendezvousConnector* rendezvousConnectorPtr,
        std::unique_ptr<UdtStreamSocket> udtConnection,
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
