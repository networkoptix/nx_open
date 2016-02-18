#ifndef nx_cc_cloud_server_socket_h
#define nx_cc_cloud_server_socket_h

#include <nx/network/abstract_socket.h>
#include <nx/network/cloud/mediator_connections.h>
#include <nx/network/cloud/tunnel/incoming_tunnel_pool.h>
#include <nx/network/socket_attributes_cache.h>

namespace nx {
namespace network {
namespace cloud {

//!Accepts connections incoming via mediator
/*!
    Listening hostname is reported to the mediator to listen on.
    \todo #ak what listening port should mean in this case?
*/
class NX_NETWORK_API CloudServerSocket
:
    public AbstractSocketAttributesCache<
        AbstractStreamServerSocket, SocketAttributes>
{
public:
    typedef std::function<std::unique_ptr<AbstractTunnelAcceptor>(
            hpm::api::ConnectionRequestedEvent&)> AcceptorMaker;

    static const std::vector<AcceptorMaker> kDefaultAcceptorMakers;

    CloudServerSocket(
        std::shared_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection,
        std::vector<AcceptorMaker> acceptorMakers = kDefaultAcceptorMakers);

    ~CloudServerSocket();

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
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    //!Implementation of AbstractSocket::*
    void post(nx::utils::MoveOnlyFunc<void()> handler) override;
    void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override;
    aio::AbstractAioThread* getAioThread() override;
    void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    //!Implementation of AbstractStreamServerSocket::acceptAsync
    void acceptAsync(
        std::function<void(SystemError::ErrorCode,
                           AbstractStreamSocket*)> handler) override;

protected:
    void initTunnelPool(int queueLen);
    void startAcceptor(std::unique_ptr<AbstractTunnelAcceptor> acceptor);

    std::shared_ptr<hpm::api::MediatorServerTcpConnection> m_mediatorConnection;
    const std::vector<AcceptorMaker> m_acceptorMakers;

    QnMutex m_mutex;
    bool m_terminated;
    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> m_acceptors;
    std::unique_ptr<IncomingTunnelPool> m_tunnelPool;
    mutable SystemError::ErrorCode m_lastError;
    std::unique_ptr<AbstractCommunicatingSocket> m_ioThreadSocket;
    std::unique_ptr<AbstractStreamSocket> m_acceptedSocket;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif  //nx_cc_cloud_server_socket_h
