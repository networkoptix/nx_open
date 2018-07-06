#pragma once

#include <memory>

#include <nx/utils/system_error.h>

#include "nx/network/aio/abstract_pollable.h"
#include "nx/network/aio/timer.h"
#include "nx/network/udt/udt_socket.h"
#include "nx/network/system_socket.h"

namespace nx {
namespace network {
namespace cloud {
namespace udp {

/**
 * Initiates rendezvous connection with given remote address.
 * NOTE: Instance can be safely freed within its aio thread (e.g., within completion handler).
 */
class RendezvousConnector:
    public aio::AbstractPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>
        ConnectCompletionHandler;

    /**
     * @param udpSocket If not empty, this socket is passed to udt socket.
     */
    RendezvousConnector(
        nx::String connectSessionId,
        SocketAddress remotePeerAddress,
        std::unique_ptr<nx::network::UDPSocket> udpSocket);
    RendezvousConnector(
        nx::String connectSessionId,
        SocketAddress remotePeerAddress,
        SocketAddress localAddressToBindTo);
    virtual ~RendezvousConnector();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    virtual void connect(
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler completionHandler);
    /**
     * Moves connection out of this.
     */
    virtual std::unique_ptr<nx::network::UdtStreamSocket> takeConnection();

    const nx::String& connectSessionId() const;
    const SocketAddress& remoteAddress() const;

protected:
    aio::Timer m_aioThreadBinder;

private:
    const nx::String m_connectSessionId;
    const SocketAddress m_remotePeerAddress;
    std::unique_ptr<nx::network::UDPSocket> m_udpSocket;
    std::unique_ptr<nx::network::UdtStreamSocket> m_udtConnection;
    ConnectCompletionHandler m_completionHandler;
    boost::optional<SocketAddress> m_localAddressToBindTo;

    bool initializeUdtConnection(
        UdtStreamSocket* udtConnection,
        std::chrono::milliseconds timeout);
    void onUdtConnectFinished(SystemError::ErrorCode errorCode);
};

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
