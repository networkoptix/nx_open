#pragma once 

#include <memory>
#include <vector>

#include <nx/network/abstract_socket.h>
#include <nx/network/aggregate_acceptor.h>
#include <nx/network/retry_timer.h>
#include <nx/network/socket_attributes_cache.h>
#include <nx/utils/basic_factory.h>

#include "mediator_server_connections.h"
#include "tunnel/incoming_tunnel_pool.h"
#include "tunnel/relay/relay_connection_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * Accepts connections incoming via mediator.
 * Listening hostname is reported to the mediator to listen on.
 */
class NX_NETWORK_API CloudServerSocket:
    public AbstractSocketAttributesCache<
        AbstractStreamServerSocket, SocketAttributes>
{
public:
    using AcceptorMaker = std::function<
        std::unique_ptr<AbstractTunnelAcceptor>(
            const hpm::api::ConnectionRequestedEvent&)>;

    static const std::vector<AcceptorMaker> kDefaultAcceptorMakers;

    CloudServerSocket(
        std::unique_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection,
        nx::network::RetryPolicy mediatorRegistrationRetryPolicy 
            = nx::network::RetryPolicy(),
        std::vector<AcceptorMaker> acceptorMakers = kDefaultAcceptorMakers);

    ~CloudServerSocket();

    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    bool close() override;
    bool isClosed() const override;
    bool shutdown() override;
    bool getLastError(SystemError::ErrorCode* errorCode) const override;
    AbstractSocket::SOCKET_HANDLE handle() const override;

    bool listen(int queueLen) override;
    AbstractStreamSocket* accept() override;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    void pleaseStopSync(bool assertIfCalledUnderLock = true) override;

    void post(nx::utils::MoveOnlyFunc<void()> handler) override;
    void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override;
    aio::AbstractAioThread* getAioThread() const override;
    void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual void cancelIOSync() override;

    bool isInSelfAioThread();

    /**
     * Invokes listen on mediator.
     */
    void registerOnMediator(
        nx::utils::MoveOnlyFunc<void(hpm::api::ResultCode)> handler);

    hpm::api::ResultCode registerOnMediatorSync();
    void setSupportedConnectionMethods(hpm::api::ConnectionMethods value);

    /**
     * For use in tests only.
     */
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
    void onListenRequestCompleted(
        nx::hpm::api::ResultCode resultCode, hpm::api::ListenResponse response);
    void startAcceptingConnections(const hpm::api::ListenResponse& response);
    void initializeCustomAcceptors(const hpm::api::ListenResponse& response);
    void retryRegistration();
    void reportResult(SystemError::ErrorCode sysErrorCode);
    AbstractStreamSocket* acceptNonBlocking();
    AbstractStreamSocket* acceptBlocking();
    void acceptAsyncInternal(AcceptCompletionHandler handler);
    void onNewConnectionHasBeenAccepted(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractStreamSocket> socket);
    void cancelAccept();

    void issueRegistrationRequest();
    void onConnectionRequested(hpm::api::ConnectionRequestedEvent event);
    void onMediatorConnectionRestored();

    std::unique_ptr<hpm::api::MediatorServerTcpConnection> m_mediatorConnection;
    nx::network::RetryTimer m_mediatorRegistrationRetryTimer;
    const std::vector<AcceptorMaker> m_acceptorMakers;
    int m_acceptQueueLen;

    std::atomic<State> m_state;
    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> m_acceptors;
    IncomingTunnelPool* m_tunnelPool = nullptr;
    std::vector<AbstractConnectionAcceptor*> m_customConnectionAcceptors;
    mutable SystemError::ErrorCode m_lastError;
    AcceptCompletionHandler m_savedAcceptHandler;
    hpm::api::ConnectionMethods m_supportedConnectionMethods = 0xFFFF; //< No limits by default.
    nx::utils::MoveOnlyFunc<void(hpm::api::ResultCode)> m_registrationHandler;
    AggregateAcceptor m_aggregateAcceptor;

private:
    void stopWhileInAioThread();
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API CustomAcceptorFactory:
    public nx::utils::BasicFactory<
        std::vector<std::unique_ptr<AbstractConnectionAcceptor>>(
            const hpm::api::ListenResponse&)>
{
    using base_type = nx::utils::BasicFactory<
        std::vector<std::unique_ptr<AbstractConnectionAcceptor>>(
            const hpm::api::ListenResponse&)>;

public:
    CustomAcceptorFactory();

    static CustomAcceptorFactory& instance();

private:
    std::vector<std::unique_ptr<AbstractConnectionAcceptor>> defaultFactoryFunc(
        const hpm::api::ListenResponse&);
};

} // namespace cloud
} // namespace network
} // namespace nx
