#include "cloud_server_socket.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/future.h>

#include "tunnel/relay/relay_connection_acceptor.h"
#include "tunnel/udp/acceptor.h"
#include "tunnel/tcp/reverse_tunnel_acceptor.h"

#define DEBUG_LOG(MESSAGE) do \
{ \
    if (nx::network::SocketGlobals::debugIni().cloudServerSocket) \
        NX_LOGX(MESSAGE, cl_logDEBUG1); \
} while (0)

namespace nx {
namespace network {
namespace cloud {

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
    m_acceptQueueLen(kDefaultAcceptQueueSize),
    m_state(State::init)
{
    bindToAioThread(getAioThread());

    // TODO: #mu default values for m_socketAttributes shall match default
    //           system vales: think how to implement this...
    m_socketAttributes.nonBlockingMode = false;
    m_socketAttributes.recvTimeout = 0;
}

CloudServerSocket::~CloudServerSocket()
{
    pleaseStopSync();

    // TODO: #ak This method should be implemented in a following way:
    //if (isInSelfAioThread())
    //    stopWhileInAioThread();
}

bool CloudServerSocket::bind(const SocketAddress& /*localAddress*/)
{
    // Does not make any sense in cloud socket context.
    return true;
}

SocketAddress CloudServerSocket::getLocalAddress() const
{
    // TODO: #ak what listening port should mean here?
    // TODO: #mux Figure out if it causes any problems
    return SocketAddress();
}

bool CloudServerSocket::close()
{
    //NX_ASSERT(false, Q_FUNC_INFO, "Not implemented...");
    return true;
}

bool CloudServerSocket::isClosed() const
{
    NX_ASSERT(false, Q_FUNC_INFO, "Not implemented...");
    return false;
}

bool CloudServerSocket::shutdown()
{
    NX_ASSERT(false, Q_FUNC_INFO, "Not implemented...");
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

AbstractStreamSocket* CloudServerSocket::accept()
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

void CloudServerSocket::pleaseStopSync(bool assertIfCalledUnderLock)
{
    if (isInSelfAioThread())
    {
        stopWhileInAioThread();
    }
    else
    {
        AbstractStreamServerSocket::pleaseStopSync(assertIfCalledUnderLock);
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
    base_type::bindToAioThread(aioThread);

    NX_ASSERT(!m_tunnelPool && m_acceptors.empty());
    m_mediatorRegistrationRetryTimer.bindToAioThread(aioThread);
    m_mediatorConnection->bindToAioThread(aioThread);
    m_aggregateAcceptor.bindToAioThread(aioThread);
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
                    m_mediatorConnection->setOnConnectionRequestedHandler(std::bind(
                        &CloudServerSocket::onConnectionRequested,
                        this,
                        std::placeholders::_1));
                    m_state = State::readyToListen;
                    /*fallthrough*/

                case State::readyToListen:
                    m_state = State::registeringOnMediator;
                    m_savedAcceptHandler = std::move(handler);
                    DEBUG_LOG("Register on mediator from acceptAsync()");
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

void CloudServerSocket::cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
{
    // Doing post to avoid calling handler within this call.
    m_mediatorRegistrationRetryTimer.post(
        [this, handler = std::move(handler)]() mutable
        {
            cancelAccept();
            handler();
        });
}

void CloudServerSocket::cancelIOSync()
{
    if (m_mediatorRegistrationRetryTimer.isInSelfAioThread())
    {
        cancelAccept();
    }
    else
    {
        // We need dispatch here to avoid blocking if called within aio thread.
        nx::utils::promise<void> cancelledPromise;
        cancelIOAsync(
            [this, &cancelledPromise]
            {
                cancelledPromise.set_value();
            });
        cancelledPromise.get_future().wait();
    }
}

bool CloudServerSocket::isInSelfAioThread() const
{
    return m_mediatorRegistrationRetryTimer.isInSelfAioThread();
}

void CloudServerSocket::registerOnMediator(
    nx::utils::MoveOnlyFunc<void(hpm::api::ResultCode)> handler)
{
    using namespace std::placeholders;

    if (m_state == State::listening)
        return handler(hpm::api::ResultCode::ok);

    if (m_state == State::init)
    {
        initTunnelPool(m_acceptQueueLen);
        m_mediatorConnection->setOnConnectionRequestedHandler(
            std::bind(&CloudServerSocket::onConnectionRequested, this, _1));
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
    using namespace std::placeholders;

    if (!m_tunnelPool)
        initTunnelPool(m_acceptQueueLen);
    m_mediatorConnection->setOnConnectionRequestedHandler(
        std::bind(&CloudServerSocket::onConnectionRequested, this, _1));
    m_state = State::listening;
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
            DEBUG_LOG(lm("Acceptor %1 returned %2: %3")
                .args((void*)acceptorPtr, (void*)connection.get(), SystemError::toString(code)));

            const auto it = std::find_if(
                m_acceptors.begin(), m_acceptors.end(),
                [&](const std::unique_ptr<AbstractTunnelAcceptor>& a)
                { return a.get() == acceptorPtr; });

            if (code == SystemError::noError)
            {
                NX_INFO(this, lm("Cloud connection (session %1) from %2 has been accepted. Info %3")
                    .args(acceptorPtr->connectionId(), acceptorPtr->remotePeerId(),
                        acceptorPtr->toString()));
            }
            else
            {
                NX_INFO(this, lm("Cloud connection (session %1) from %2 has not been accepted with error %3. Info %4")
                    .args(acceptorPtr->connectionId(), acceptorPtr->remotePeerId(),
                        SystemError::toString(code), acceptorPtr->toString()));
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
    const auto registrationHandlerGuard = makeScopeGuard(
        [handler = std::move(m_registrationHandler), resultCode]()
        {
            if (handler)
                handler(resultCode);
        });

    m_registrationHandler = nullptr;
    NX_ASSERT(m_state == State::registeringOnMediator);
    if (resultCode == nx::hpm::api::ResultCode::ok)
    {
        NX_LOGX(lm("Listen request completed successfully"), cl_logDEBUG1);
        m_state = State::listening;
        startAcceptingConnections(response);
        return;
    }

    NX_LOGX(lm("Listen request has failed: %1").arg(resultCode), cl_logDEBUG1);

    if (!m_mediatorRegistrationRetryTimer.scheduleNextTry(
            std::bind(&CloudServerSocket::retryRegistration, this)))
    {
        // It is not supposed to happen in production, is it?
        NX_LOGX(lm("Stopped mediator registration retries. Last result code %1")
            .arg(resultCode), cl_logWARNING);
        m_state = State::readyToListen;
        reportResult(SystemError::invalidData); //< TODO: #ak Use better error code.
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
    NX_LOGX(lm("Retrying to register on mediator"), cl_logDEBUG1);
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
        m_aggregateAcceptor.setAcceptTimeout(boost::none);
    }

    m_aggregateAcceptor.acceptAsync(
        std::bind(&CloudServerSocket::onNewConnectionHasBeenAccepted, this, _1, _2));
}

AbstractStreamSocket* CloudServerSocket::acceptNonBlocking()
{
    if (auto socket = m_tunnelPool->getNextSocketIfAny())
        return socket.release();
    for (auto& acceptor: m_customConnectionAcceptors)
    {
        if (auto socket = acceptor->getNextSocketIfAny())
            return socket.release();
    }

    SystemError::setLastErrorCode(SystemError::wouldBlock);
    return nullptr;
}

AbstractStreamSocket* CloudServerSocket::acceptBlocking()
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

    return acceptedSocket.release();
}

void CloudServerSocket::onNewConnectionHasBeenAccepted(
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractStreamSocket> socket)
{
    decltype(m_savedAcceptHandler) handler;
    handler.swap(m_savedAcceptHandler);

    if (socket)
        socket->bindToAioThread(SocketGlobals::aioService().getRandomAioThread());

    NX_LOGX(lm("Returning socket from tunnel pool. Result code %1")
        .arg(SystemError::toString(sysErrorCode)), cl_logDEBUG2);
    handler(sysErrorCode, std::move(socket));
}

void CloudServerSocket::cancelAccept()
{
    m_aggregateAcceptor.cancelIOSync();
    m_savedAcceptHandler = nullptr;
}

void CloudServerSocket::issueRegistrationRequest()
{
    using namespace std::placeholders;

    const auto cloudCredentials = 
        m_mediatorConnection->credentialsProvider()->getSystemCredentials();

    if (!cloudCredentials)  //< TODO: #ak this MUST be assert.
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
    DEBUG_LOG(lm("Connection request '%1' from %2 with methods: %3")
        .args(event.connectSessionId, event.originatingPeerID,
            hpm::api::ConnectionMethod::toString(event.connectionMethods)));

    auto acceptors = TunnelAcceptorFactory::instance().create(
        *m_mediatorConnector->udpEndpoint(),
        event);
    for (auto& acceptor: acceptors)
    {
        DEBUG_LOG(lm("Create acceptor '%1' by connection request %2 from %3")
            .args(acceptor, event.connectSessionId, event.originatingPeerID));

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

        NX_LOGX(lm("Register on mediator after reconnect"), cl_logDEBUG1);
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
        QUrl trafficRelayUrlWithCredentials = QString::fromUtf8(trafficRelayUrl);
        trafficRelayUrlWithCredentials.setUserName(credentials.hostName());
        trafficRelayUrlWithCredentials.setPassword(credentials.key);
        acceptors.push_back(
            std::make_unique<relay::ConnectionAcceptor>(
                trafficRelayUrlWithCredentials));
    }

    return acceptors;
}

} // namespace cloud
} // namespace network
} // namespace nx
