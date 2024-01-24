// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <nx/network/abstract_socket.h>
#include <nx/network/aggregate_acceptor.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/retry_timer.h>
#include <nx/network/socket_attributes_cache.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/basic_factory.h>

#include "mediator_connector.h"
#include "mediator_server_connections.h"
#include "tunnel/incoming_tunnel_pool.h"
#include "tunnel/relay/relay_connection_acceptor.h"
#include "tunnel/tunnel_acceptor_factory.h"

namespace nx::network::cloud {

struct CloudConnectListenerStatusReport
{
    template<typename ResultType>
    struct Error
    {
        nx::utils::Url url;
        ResultType result;
    };

    /**
     * true if listening for incoming cloud connections. Note that it is possible that some errors
     * still occurred, but the socket is still listening.
     */
    bool listening = false;

    /**
     * The list of errors connecting to a mediator encountered during the last listen attempt.
     */
    std::vector<Error<nx::hpm::api::Result>> mediatorErrors;

    /** Errors while interracting with relays. */
    std::vector<Error<nx::cloud::relay::api::Result>> relayErrors;
};

/**
 * Accepts connections incoming via mediator.
 * Listening hostname is reported to the mediator to listen on.
 */
class NX_NETWORK_API CloudServerSocket:
    public AbstractSocketAttributesCache<
        AbstractStreamServerSocket, SocketAttributes>
{
    using base_type = AbstractSocketAttributesCache<
        AbstractStreamServerSocket, SocketAttributes>;

public:
    using AcceptorMaker = std::function<
        std::unique_ptr<AbstractTunnelAcceptor>(
            const hpm::api::ConnectionRequestedEvent&)>;

    static const std::vector<AcceptorMaker> kDefaultAcceptorMakers;

    CloudServerSocket(
        hpm::api::AbstractMediatorConnector* mediatorConnector,
        nx::network::RetryPolicy mediatorRegistrationRetryPolicy
            = nx::network::RetryPolicy());

    ~CloudServerSocket();

    bool bind(const SocketAddress& localAddress) override;
    SocketAddress getLocalAddress() const override;
    bool close() override;
    bool isClosed() const override;
    bool shutdown() override;
    bool getLastError(SystemError::ErrorCode* errorCode) const override;
    virtual bool getProtocol(int* protocol) const override;
    AbstractSocket::SOCKET_HANDLE handle() const override;

    bool listen(int queueLen) override;
    std::unique_ptr<AbstractStreamSocket> accept() override;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    void pleaseStopSync() override;

    void post(nx::utils::MoveOnlyFunc<void()> handler) override;
    void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override;
    aio::AbstractAioThread* getAioThread() const override;
    void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;

    virtual bool isInSelfAioThread() const override;

    /**
     * Invokes listen on mediator.
     */
    void registerOnMediator(
        nx::utils::MoveOnlyFunc<void(hpm::api::ResultCode)> handler);

    hpm::api::ResultCode registerOnMediatorSync();
    void setSupportedConnectionMethods(hpm::api::ConnectionMethods value);

    /**
     * Provides the current status of the listener. Note that the list of errors is cleared on
     * every listen attempt.
     */
    CloudConnectListenerStatusReport getStatusReport() const;

    /**
     * For use in tests only.
     */
    void moveToListeningState();

protected:
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(State,
        init,
        readyToListen,
        registeringOnMediator,
        listening
    );

    virtual void cancelIoInAioThread() override;

    void bindToAioThreadUnchecked(aio::AbstractAioThread* aioThread);
    void initTunnelPool(int queueLen);
    void startAcceptor(std::unique_ptr<AbstractTunnelAcceptor> acceptor);
    void onListenRequestCompleted(
        nx::hpm::api::ResultCode resultCode, hpm::api::ListenResponse response);
    void startAcceptingConnections(const hpm::api::ListenResponse& response);
    void initializeCustomAcceptors(const hpm::api::ListenResponse& response);
    void retryRegistration();
    void reportResult(SystemError::ErrorCode systemErrorCode);
    std::unique_ptr<AbstractStreamSocket> acceptNonBlocking();
    std::unique_ptr<AbstractStreamSocket> acceptBlocking();
    void acceptAsyncInternal(AcceptCompletionHandler handler);
    void onNewConnectionHasBeenAccepted(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractStreamSocket> socket);

    void issueRegistrationRequest();
    void onConnectionRequested(hpm::api::ConnectionRequestedEvent event);
    void onMediatorConnectionRestored();

    hpm::api::AbstractMediatorConnector* m_mediatorConnector;
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> m_mediatorConnection;
    nx::network::RetryTimer m_mediatorRegistrationRetryTimer;
    int m_acceptQueueLen;

    State m_state = State::init;
    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> m_acceptors;
    IncomingTunnelPool* m_tunnelPool = nullptr;
    std::vector<AbstractConnectionAcceptor*> m_customConnectionAcceptors;
    mutable SystemError::ErrorCode m_lastError;
    AcceptCompletionHandler m_savedAcceptHandler;
    hpm::api::ConnectionMethods m_supportedConnectionMethods = 0xFFFF; //< No limits by default.
    nx::utils::MoveOnlyFunc<void(hpm::api::ResultCode)> m_registrationHandler;
    AggregateAcceptor m_aggregateAcceptor;
    mutable nx::Mutex m_mutex;
    CloudConnectListenerStatusReport m_lastListenStatusReport;

private:
    void stopWhileInAioThread();
    void saveSuccessListenStartToStatusReport();
    void saveMediatorErrorToStatusReport(nx::hpm::api::ResultCode resultCode, bool clearReport);
};

//-------------------------------------------------------------------------------------------------

using CustomAcceptorFactoryFunction =
    std::vector<std::unique_ptr<AbstractConnectionAcceptor>>(
        const nx::hpm::api::SystemCredentials&,
        const hpm::api::ListenResponse&);

class NX_NETWORK_API CustomAcceptorFactory:
    public nx::utils::BasicFactory<CustomAcceptorFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<CustomAcceptorFactoryFunction>;

public:
    CustomAcceptorFactory();

    static CustomAcceptorFactory& instance();

private:
    std::vector<std::unique_ptr<AbstractConnectionAcceptor>> defaultFactoryFunction(
        const nx::hpm::api::SystemCredentials&,
        const hpm::api::ListenResponse&);
};

} // namespace nx::network::cloud
