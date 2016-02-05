#ifndef nx_cc_cloud_server_socket_h
#define nx_cc_cloud_server_socket_h

#include <nx/utils/async_operation_guard.h>
#include <nx/network/abstract_socket.h>

#include "tunnel/incoming_tunnel.h"

namespace nx {
namespace network {
namespace cloud {

//!Accepts connections incoming via mediator
/*!
    Listening hostname is reported to the mediator to listen on.
    \todo #ak what listening port should mean in this case?
*/
class NX_NETWORK_API CloudServerSocket:
    public AbstractSocketAttributesCache<
        AbstractStreamServerSocket, StreamSocketAttributes>
{
public:
    CloudServerSocket(
        std::shared_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection,
        IncomingTunnelPool* tunnelPool);

    //!Implementation of AbstractSocket::*
    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    void close() override;
    bool isClosed() const override;
    void shutdown() override;
    bool getLastError(SystemError::ErrorCode* errorCode) const override;
    AbstractSocket::SOCKET_HANDLE handle() const override;

    //!Implementation of AbstractStreamServerSocket::*
    bool listen(int queueLen) override;
    AbstractStreamSocket* accept() override;

    //!Implementation of QnStoppable::pleaseStop
    void pleaseStop(std::function<void()> handler) override;

    //!Implementation of AbstractSocket::*
    void post(std::function<void()> handler) override;
    void dispatch(std::function<void()> handler) override;
    aio::AbstractAioThread* getAioThread() override;
    void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    //!Implementation of AbstractStreamServerSocket::acceptAsync
    void acceptAsync(
        std::function<void(SystemError::ErrorCode,
                           AbstractStreamSocket*)> handler) override;

protected:
    void startAcceptor(
        std::unique_ptr<AbstractTunnelAcceptor> connector,
        std::shared_ptr<utils::AsyncOperationGuard::SharedGuard> sharedGuard);

    void callAcceptHandler();

    const std::shared_ptr<hpm::api::MediatorServerTcpConnection> m_mediatorConnection;
    IncomingTunnelPool* const m_tunnelPool;

    typedef AbstractTunnelAcceptor Acceptor;
    std::map<Acceptor*, std::unique_ptr<Acceptor>> m_acceptors;

    mutable SystemError::ErrorCode m_lastError;
    std::unique_ptr<AbstractCommunicatingSocket> m_ioThreadSocket;
    std::unique_ptr<AbstractCommunicatingSocket> m_timerThreadSocket;
    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> m_acceptHandler;
    std::unique_ptr<AbstractStreamSocket> m_acceptedSocket;
    utils::AsyncOperationGuard m_asyncGuard;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif  //nx_cc_cloud_server_socket_h
