#ifndef nx_cc_cloud_server_socket_h
#define nx_cc_cloud_server_socket_h

#include <nx/network/abstract_socket.h>
#include <nx/network/cloud/mediator_connections.h>
#include <nx/network/cloud/tunnel/incoming_tunnel_pool.h>
#include <nx/network/retry_timer.h>
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
    typedef std::function<
                std::unique_ptr<AbstractTunnelAcceptor>(
                    const hpm::api::ConnectionRequestedEvent&)> AcceptorMaker;

    static const std::vector<AcceptorMaker> kDefaultAcceptorMakers;

    CloudServerSocket(
        std::unique_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection,
        nx::network::RetryPolicy mediatorRegistrationRetryPolicy 
            = nx::network::RetryPolicy(),
        std::vector<AcceptorMaker> acceptorMakers = kDefaultAcceptorMakers);

    ~CloudServerSocket();

    //!Implementation of AbstractSocket::*
    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    bool close() override;
    bool isClosed() const override;
    bool shutdown() override;
    bool getLastError(SystemError::ErrorCode* errorCode) const override;
    AbstractSocket::SOCKET_HANDLE handle() const override;

    //!Implementation of AbstractStreamServerSocket::*
    bool listen(int queueLen) override;
    AbstractStreamSocket* accept() override;

    //!Implementation of QnStoppable::pleaseStop
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    void pleaseStopSync(bool assertIfCalledUnderLock) override;

    //!Implementation of AbstractSocket::*
    void post(nx::utils::MoveOnlyFunc<void()> handler) override;
    void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override;
    aio::AbstractAioThread* getAioThread() const override;
    void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    //!Implementation of AbstractStreamServerSocket::acceptAsync
    virtual void acceptAsync(
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            AbstractStreamSocket*)> handler) override;
    //!Implementation of AbstractStreamServerSocket::cancelIOAsync
    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) override;
    //!Implementation of AbstractStreamServerSocket::cancelIOSync
    virtual void cancelIOSync() override;

    /** Invokes listen on mediator */
    void registerOnMediator(
        nx::utils::MoveOnlyFunc<void(hpm::api::ResultCode)> handler);

    hpm::api::ResultCode registerOnMediatorSync();
    void setSupportedConnectionMethods(hpm::api::ConnectionMethods value);

    /** test only */
    void moveToListeningState();

protected:
    enum class State
    {
        init,
        readyToListen,
        registeringOnMediator,
        listening
    };

    void initTunnelPool(int queueLen);
    void startAcceptor(std::unique_ptr<AbstractTunnelAcceptor> acceptor);
    void onListenRequestCompleted(nx::hpm::api::ResultCode resultCode);
    void acceptAsyncInternal(
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode code,
            AbstractStreamSocket*)> handler);
    void issueRegistrationRequest();
    void onConnectionRequested(hpm::api::ConnectionRequestedEvent event);
    void onMediatorConnectionRestored();

    std::unique_ptr<hpm::api::MediatorServerTcpConnection> m_mediatorConnection;
    nx::network::RetryTimer m_mediatorRegistrationRetryTimer;
    const std::vector<AcceptorMaker> m_acceptorMakers;
    int m_acceptQueueLen;

    std::atomic<State> m_state;
    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> m_acceptors;
    std::unique_ptr<IncomingTunnelPool> m_tunnelPool;
    mutable SystemError::ErrorCode m_lastError;
    std::unique_ptr<AbstractStreamSocket> m_acceptedSocket;
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode code,
        AbstractStreamSocket*)> m_savedAcceptHandler;
    hpm::api::ConnectionMethods m_supportedConnectionMethods = 0xFFFF; //< No limits by default

private:
    void stopWhileInAioThread();
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif  //nx_cc_cloud_server_socket_h
