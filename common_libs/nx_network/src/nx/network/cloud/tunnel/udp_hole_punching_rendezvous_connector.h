/**********************************************************
* Apr 13, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <memory>

#include <utils/common/systemerror.h>

#include "nx/network/aio/abstract_pollable.h"
#include "nx/network/aio/timer.h"
#include "nx/network/udt/udt_socket.h"
#include "nx/network/system_socket.h"


namespace nx {
namespace network {
namespace cloud {

/** Initiates rendezvous connection with given remote address.
    Also, verifies that remote side is using same connection id
    \note Instance can be safely freed within its aio thread (e.g., within completion handler)
*/
class UdpHolePunchingRendezvousConnector
:
    public aio::AbstractPollable
{
public:
    /**
        @param udpSocket If not empty, this socket is passed to udt socket
    */
    UdpHolePunchingRendezvousConnector(
        nx::String connectSessionId,
        SocketAddress remotePeerAddress,
        std::unique_ptr<nx::network::UDPSocket> udpSocket);
    virtual ~UdpHolePunchingRendezvousConnector();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    virtual aio::AbstractAioThread* getAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    /**
        @param completionHandler Success is returned only if remote side is aware of connection id
    */
    void connect(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            std::unique_ptr<UdtStreamSocket>)> completionHandler);

    SocketAddress remoteAddress() const;

private:
    aio::Timer m_aioThreadBinder;
    const nx::String m_connectSessionId;
    const SocketAddress m_remotePeerAddress;
    std::unique_ptr<nx::network::UDPSocket> m_udpSocket;
    std::unique_ptr<nx::network::UdtStreamSocket> m_udtConnection;
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<UdtStreamSocket>)> m_completionHandler;

    void onUdtConnectFinished(SystemError::ErrorCode errorCode);
};

} // namespace cloud
} // namespace network
} // namespace nx
