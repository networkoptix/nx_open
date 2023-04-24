// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_server_socket.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/future.h>

#include "tunnel/relay/relay_connection_acceptor.h"
#include "tunnel/udp/acceptor.h"

namespace nx::network::cloud {

namespace {

const int kDefaultAcceptQueueSize = 128;
const KeepAliveOptions kDefaultKeepAlive(std::chrono::minutes(1), std::chrono::seconds(10), 5);

} // namespace

CloudServerSocket::CloudServerSocket(
    hpm::api::AbstractMediatorConnector* mediatorConnector,
    nx::network::RetryPolicy mediatorRegistrationRetryPolicy)
:
    m_mediatorConnector(mediatorConnector),
    m_mediatorConnection(mediatorConnector->systemConnection()),
    m_mediatorRegistrationRetryTimer(std::move(mediatorRegistrationRetryPolicy)),
    m_acceptQueueLen(kDefaultAcceptQueueSize)
{
    bindToAioThreadUnchecked(getAioThread());

    m_socketAttributes.nonBlockingMode = false;
    m_socketAttributes.recvTimeout = 0;
}

CloudServerSocket::~CloudServerSocket()
{
    pleaseStopSync();
}

bool CloudServerSocket::bind(const SocketAddress& /*localAddress*/)
{
    // Does not make any sense in cloud socket context.
    return true;
}

SocketAddress CloudServerSocket::getLocalAddress() const
{
    // TODO: #akolesnikov what listening port should mean here?
    // TODO: #muskov Figure out if it causes any problems
    return SocketAddress();
}

bool CloudServerSocket::close()
{
    // TODO: #akolesnikov Not implemented.
    return true;
}

bool CloudServerSocket::isClosed() const
{
    return false;
}

bool CloudServerSocket::shutdown()
{
    // TODO: #akolesnikov Not implemented.
    NX_ASSERT(false, "Not implemented...");
    return true;
}

bool CloudServerSocket::getLastError(SystemError::ErrorCode* errorCode) const
{
    if (m_lastError == SystemError::noError)
        return false;

    *errorCode = m_lastError;
    m_lastError = SystemError::noError;
    return true;
}

bool CloudServerSocket::getProtocol(int* protocol) const
{
    *protocol = nx::network::Protocol::unassigned;
    return true;
}

AbstractSocket::SOCKET_HANDLE CloudServerSocket::handle() const
{
    NX_ASSERT(false);
    return (AbstractSocket::SOCKET_HANDLE)(-1);
}

bool CloudServerSocket::listen(int queueLen)
{
    // Actual initialization is done with first accept call.
    m_acceptQueueLen = queueLen != 0 ? queueLen : kDefaultAcceptQueueSize;
    return true;
}

std::unique_ptr<AbstractStreamSocket> CloudServerSocket::accept()
{
    NX_CRITICAL(!SocketGlobals::aioService().isInAnyAioThread());

    if (m_socketAttributes.nonBlockingMode && *m_socketAttributes.nonBlockingMode)
        return acceptNonBlocking();

    return acceptBlocking();
}

void CloudServerSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_mediatorRegistrationRetryTimer.post(
        [this, handler = std::move(handler)]()
        {
            stopWhileInAioThread();
            handler();
        });
}

void CloudServerSocket::pleaseStopSync()
{
    if (isInSelfAioThread())
    {
        stopWhileInAioThread();
    }
    else
    {
        AbstractStreamServerSocket::pleaseStopSync();
    }
}

void CloudServerSocket::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_mediatorRegistrationRetryTimer.post(std::move(handler));
}

void CloudServerSocket::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_mediatorRegistrationRetryTimer.dispatch(std::move(handler));
}

aio::AbstractAioThread* CloudServerSocket::getAioThread() const
{
    return m_mediatorRegistrationRetryTimer.getAioThread();
}

void CloudServerSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    if (getAioThread() == aioThread)
        return;

    bindToAioThreadUnchecked(aioThread);
}

void CloudServerSocket::acceptAsync(AcceptCompletionHandler handler)
{
    dispatch(
        [this, handler = std::move(handler)]() mutable
        {
            switch (m_state)
            {
                case State::init:
                    initTunnelPool(m_acceptQueueLen);
                    m_mediatorConnection->setOnConnectionRequestedHandler(
                        [this](auto&&... args) {
                            onConnectionRequested(std::forward<decltype(args)>(args)...);
                        });
                    m_state = State::readyToListen;
                    NX_VERBOSE(this, "Moved to %1", m_state);
                    [[fallthrough]];

                case State::readyToListen:
                    m_state = State::registeringOnMediator;
                    m_savedAcceptHandler = std::move(handler);
                    NX_VERBOSE(this, "Register on mediator from acceptAsync()");
                    issueRegistrationRequest();
                    break;

                case State::registeringOnMediator:
                    NX_CRITICAL(!m_savedAcceptHandler);
                    m_savedAcceptHandler = std::move(handler);
                    break;

                case State::listening:
                    acceptAsyncInternal(std::move(handler));
                    break;
            }
        });
}

void CloudServerSocket::cancelIoInAioThread()
{
    m_aggregateAcceptor.cancelIOSync();
    m_savedAcceptHandler = nullptr;
}

bool CloudServerSocket::isInSelfAioThread() const
{
    return m_mediatorRegistrationRetryTimer.isInSelfAioThread();
}

void CloudServerSocket::registerOnMediator(
    nx::utils::MoveOnlyFunc<void(hpm::api::ResultCode)> handler)
{
    NX_VERBOSE(this, "Starting registration on the mediator. Current state %1", m_state);

    if (m_state == State::listening)
        return handler(hpm::api::ResultCode::ok);

    if (m_state == State::init)
    {
        initTunnelPool(m_acceptQueueLen);
        m_mediatorConnection->setOnConnectionRequestedHandler(
            [this](auto&&... args) {
                onConnectionRequested(std::forward<decltype(args)>(args)...);
            });
        m_state = State::readyToListen;
    }

    NX_ASSERT(m_state == State::readyToListen);
    m_state = State::registeringOnMediator;

    m_registrationHandler = std::move(handler);
    issueRegistrationRequest();
}

hpm::api::ResultCode CloudServerSocket::registerOnMediatorSync()
{
    nx::utils::promise<hpm::api::ResultCode> promise;
    registerOnMediator(
        [&promise](hpm::api::ResultCode code) { promise.set_value(code); });

    return promise.get_future().get();
}

void CloudServerSocket::setSupportedConnectionMethods(
    hpm::api::ConnectionMethods value)
{
    m_supportedConnectionMethods = value;
}

void CloudServerSocket::moveToListeningState()
{
    NX_VERBOSE(this, "Moving to the listening state");

    if (!m_tunnelPool)
        initTunnelPool(m_acceptQueueLen);
    m_mediatorConnection->setOnConnectionRequestedHandler(
        [this](auto&&... args) {
            onConnectionRequested(std::forward<decltype(args)>(args)...);
        });
    m_state = State::listening;
}

void CloudServerSocket::bindToAioThreadUnchecked(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    NX_ASSERT(m_acceptors.empty());
    if (m_tunnelPool)
        m_tunnelPool->bindToAioThread(aioThread);
    m_mediatorRegistrationRetryTimer.bindToAioThread(aioThread);
    m_mediatorConnection->bindToAioThread(aioThread);
    m_aggregateAcceptor.bindToAioThread(aioThread);
}

void CloudServerSocket::initTunnelPool(int queueLen)
{
    auto tunnelPool = std::make_unique<IncomingTunnelPool>(getAioThread(), queueLen);
    m_tunnelPool = tunnelPool.get();
    m_aggregateAcceptor.add(std::move(tunnelPool));
}

void CloudServerSocket::startAcceptor(
    std::unique_ptr<AbstractTunnelAcceptor> acceptor)
{
    auto acceptorPtr = acceptor.get();
    m_acceptors.push_back(std::move(acceptor));
    acceptorPtr->accept(
        [this, acceptorPtr](
            SystemError::ErrorCode code,
            std::unique_ptr<AbstractIncomingTunnelConnection> connection)
        {
            NX_ASSERT(m_mediatorConnection->isInSelfAioThread());
            NX_VERBOSE(this, "Acceptor %1 returned %2: %3",
                (void*)acceptorPtr, (void*)connection.get(), SystemError::toString(code));

            const auto it = std::find_if(
                m_acceptors.begin(), m_acceptors.end(),
                [&](const std::unique_ptr<AbstractTunnelAcceptor>& a)
                { return a.get() == acceptorPtr; });

            if (code == SystemError::noError)
            {
                NX_DEBUG(this, "Cloud connection (session %1) from %2 has been accepted. Info %3",
                    acceptorPtr->connectionId(), acceptorPtr->remotePeerId(), acceptorPtr->toString());
            }
            else
            {
                NX_WARNING(this, "Cloud connection (session %1) from %2 has not been accepted with error %3. Info %4",
                    acceptorPtr->connectionId(), acceptorPtr->remotePeerId(), SystemError::toString(code),
                    acceptorPtr->toString());
            }

            NX_CRITICAL(it != m_acceptors.end());
            m_acceptors.erase(it);

            if (code == SystemError::noError)
                m_tunnelPool->addNewTunnel(std::move(connection));
        });
}

void CloudServerSocket::onListenRequestCompleted(
    nx::hpm::api::ResultCode resultCode,
    hpm::api::ListenResponse response)
{
    const auto registrationHandlerGuard = nx::utils::makeScopeGuard(
        [handler = std::move(m_registrationHandler), resultCode]()
        {
            if (handler)
                handler(resultCode);
        });

    m_registrationHandler = nullptr;
    NX_ASSERT(m_state == State::registeringOnMediator, nx::format("m_state = %1", m_state));
    if (resultCode == nx::hpm::api::ResultCode::ok)
    {
        NX_DEBUG(this, "Listen request completed successfully");
        m_state = State::listening;
        startAcceptingConnections(response);
        return;
    }

    NX_DEBUG(this, "Listen request has failed: %1", resultCode);

    if (!m_mediatorRegistrationRetryTimer.scheduleNextTry(
            std::bind(&CloudServerSocket::retryRegistration, this)))
    {
        // It is not supposed to happen in production, is it?
        NX_WARNING(this, "Stopped mediator registration retries. Last result code %1", resultCode);
        m_state = State::readyToListen;
        reportResult(SystemError::invalidData); //< TODO: #akolesnikov Use better error code.
    }
}

void CloudServerSocket::startAcceptingConnections(
    const hpm::api::ListenResponse& response)
{
    m_mediatorConnection->setOnReconnectedHandler(
        std::bind(&CloudServerSocket::onMediatorConnectionRestored, this));

    // This is important to know if connection is lost,
    // so server will use some keep alive even if mediator does not ask it to use any.
    const auto keepAliveOptions = response.tcpConnectionKeepAlive
        ? *response.tcpConnectionKeepAlive : kDefaultKeepAlive;

    if (response.cloudConnectOptions.testFlag(hpm::api::serverChecksConnectionState))
        m_mediatorConnection->monitorListeningState(keepAliveOptions.maxDelay());
    else
        m_mediatorConnection->client()->setKeepAliveOptions(keepAliveOptions);

    initializeCustomAcceptors(response);

    if (m_savedAcceptHandler)
    {
        decltype(m_savedAcceptHandler) acceptHandler;
        acceptHandler.swap(m_savedAcceptHandler);
        acceptAsyncInternal(std::move(acceptHandler));
    }
}

void CloudServerSocket::initializeCustomAcceptors(
    const hpm::api::ListenResponse& response)
{
    const auto cloudCredentials =
        m_mediatorConnection->credentialsProvider()->getSystemCredentials();
    NX_ASSERT(cloudCredentials);
    if (!cloudCredentials)
        return;

    auto acceptors = CustomAcceptorFactory::instance().create(
        *cloudCredentials,
        response);
    for (auto& acceptor: acceptors)
    {
        acceptor->bindToAioThread(getAioThread());
        m_customConnectionAcceptors.push_back(acceptor.get());
        m_aggregateAcceptor.add(std::move(acceptor));
    }
}

void CloudServerSocket::retryRegistration()
{
    NX_DEBUG(this, "Retrying to register on mediator");
    issueRegistrationRequest();
}

void CloudServerSocket::reportResult(SystemError::ErrorCode systemErrorCode)
{
    if (!m_savedAcceptHandler)
        return;

    decltype(m_savedAcceptHandler) acceptHandler;
    acceptHandler.swap(m_savedAcceptHandler);
    acceptHandler(systemErrorCode, nullptr);
}

void CloudServerSocket::acceptAsyncInternal(AcceptCompletionHandler handler)
{
    using namespace std::placeholders;

    NX_ASSERT(isInSelfAioThread());

    m_savedAcceptHandler = std::move(handler);

    if (m_socketAttributes.recvTimeout)
    {
        m_aggregateAcceptor.setAcceptTimeout(
            std::chrono::milliseconds(*m_socketAttributes.recvTimeout));
    }
    else
    {
        m_aggregateAcceptor.setAcceptTimeout(std::nullopt);
    }

    m_aggregateAcceptor.acceptAsync(
        std::bind(&CloudServerSocket::onNewConnectionHasBeenAccepted, this, _1, _2));
}

std::unique_ptr<AbstractStreamSocket> CloudServerSocket::acceptNonBlocking()
{
    if (auto socket = m_tunnelPool->getNextSocketIfAny())
        return socket;
    for (auto& acceptor: m_customConnectionAcceptors)
    {
        if (auto socket = acceptor->getNextSocketIfAny())
            return socket;
    }

    SystemError::setLastErrorCode(SystemError::wouldBlock);
    return nullptr;
}

std::unique_ptr<AbstractStreamSocket> CloudServerSocket::acceptBlocking()
{
    nx::utils::promise<SystemError::ErrorCode> promise;
    std::unique_ptr<AbstractStreamSocket> acceptedSocket;
    acceptAsync(
        [&](SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> socket)
        {
            acceptedSocket = std::move(socket);
            promise.set_value(code);
        });

    const auto errorCode = promise.get_future().get();
    if (errorCode != SystemError::noError)
    {
        SystemError::setLastErrorCode(errorCode);
        return nullptr;
    }

    return acceptedSocket;
}

void CloudServerSocket::onNewConnectionHasBeenAccepted(
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractStreamSocket> socket)
{
    decltype(m_savedAcceptHandler) handler;
    handler.swap(m_savedAcceptHandler);

    if (socket)
        socket->bindToAioThread(SocketGlobals::aioService().getRandomAioThread());

    NX_VERBOSE(this, "Returning socket from tunnel pool. Result code %1",
        SystemError::toString(sysErrorCode));
    handler(sysErrorCode, std::move(socket));
}

void CloudServerSocket::issueRegistrationRequest()
{
    using namespace std::placeholders;

    const auto cloudCredentials =
        m_mediatorConnection->credentialsProvider()->getSystemCredentials();

    if (!cloudCredentials)  //< TODO: #akolesnikov this MUST be assert.
    {
        // Specially for unit tests.
        m_mediatorRegistrationRetryTimer.dispatch(
            std::bind(&CloudServerSocket::onListenRequestCompleted, this,
                nx::hpm::api::ResultCode::notAuthorized, hpm::api::ListenResponse()));
        return;
    }

    nx::hpm::api::ListenRequest listenRequestData;
    listenRequestData.systemId = cloudCredentials->systemId;
    listenRequestData.serverId = cloudCredentials->serverId;
    m_mediatorConnection->listen(
        std::move(listenRequestData),
        std::bind(&CloudServerSocket::onListenRequestCompleted, this, _1, _2));
}

void CloudServerSocket::onConnectionRequested(
    hpm::api::ConnectionRequestedEvent event)
{
    event.connectionMethods &= m_supportedConnectionMethods;
    NX_VERBOSE(this, "Connection request '%1' from %2 with methods: %3",
        event.connectSessionId, event.originatingPeerID,
            hpm::api::ConnectionMethod::toString(event.connectionMethods));

    auto acceptors = TunnelAcceptorFactory::instance().create(
        m_mediatorConnector->address()
            ? std::make_optional(m_mediatorConnector->address()->stunUdpEndpoint)
            : std::nullopt,
        event);
    for (auto& acceptor: acceptors)
    {
        NX_VERBOSE(this, "Create acceptor '%1' by connection request %2 from %3",
            acceptor, event.connectSessionId, event.originatingPeerID);

        acceptor->setConnectionInfo(
            event.connectSessionId, event.originatingPeerID);

        acceptor->setMediatorConnection(m_mediatorConnection.get());
        startAcceptor(std::move(acceptor));
    }
}

void CloudServerSocket::onMediatorConnectionRestored()
{
    NX_ASSERT(isInSelfAioThread());

    if (m_state == State::listening)
    {
        m_aggregateAcceptor.cancelIOSync();

        m_state = State::registeringOnMediator;
        m_mediatorRegistrationRetryTimer.reset();

        for (const auto& customAcceptor: m_customConnectionAcceptors)
            m_aggregateAcceptor.remove(customAcceptor);
        m_customConnectionAcceptors.clear();

        NX_DEBUG(this, "Register on mediator after reconnect");
        issueRegistrationRequest();
    }
}

void CloudServerSocket::stopWhileInAioThread()
{
    m_mediatorRegistrationRetryTimer.pleaseStopSync();
    m_mediatorConnection->pleaseStopSync();
    m_acceptors.clear();
    m_aggregateAcceptor.pleaseStopSync();
    m_tunnelPool = nullptr;
    m_customConnectionAcceptors.clear();
}

//-------------------------------------------------------------------------------------------------

CustomAcceptorFactory::CustomAcceptorFactory():
    base_type(std::bind(&CustomAcceptorFactory::defaultFactoryFunction, this,
        std::placeholders::_1, std::placeholders::_2))
{
}

CustomAcceptorFactory& CustomAcceptorFactory::instance()
{
    static CustomAcceptorFactory staticInstance;
    return staticInstance;
}

std::vector<std::unique_ptr<AbstractConnectionAcceptor>>
    CustomAcceptorFactory::defaultFactoryFunction(
        const nx::hpm::api::SystemCredentials& credentials,
        const hpm::api::ListenResponse& response)
{
    std::vector<std::unique_ptr<AbstractConnectionAcceptor>> acceptors;

    for (const auto& trafficRelayUrl: response.trafficRelayUrls)
    {
        const auto trafficRelayUrlWithCredentials = url::Builder(trafficRelayUrl)
            .setUserName(credentials.hostName()).setPassword(credentials.key).toUrl();
        auto acceptor = std::make_unique<relay::ConnectionAcceptor>(
            trafficRelayUrlWithCredentials);
        acceptor->setConnectTimeout(response.trafficRelayConnectTimeout);
        acceptors.push_back(std::move(acceptor));
    }

    return acceptors;
}

} // namespace nx::network::cloud
