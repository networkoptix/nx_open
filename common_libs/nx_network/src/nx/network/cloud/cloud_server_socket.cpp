
#include "cloud_server_socket.h"

#include <nx/network/socket_global.h>
#include <nx/network/stream_socket_wrapper.h>
#include <nx/utils/std/future.h>
#include <utils/serialization/lexical.h>

#include "tunnel/udp/acceptor.h"


namespace nx {
namespace network {
namespace cloud {

namespace {
const int kDefaultAcceptQueueSize = 128;
}

static const std::vector<CloudServerSocket::AcceptorMaker> defaultAcceptorMakers()
{
    std::vector<CloudServerSocket::AcceptorMaker> makers;

    makers.push_back(
        [](const hpm::api::ConnectionRequestedEvent& event)
        {
            using namespace hpm::api::ConnectionMethod;
            if (event.connectionMethods & udpHolePunching)
            {
                NX_ASSERT(event.udpEndpointList.size() == 1);
                if (!event.udpEndpointList.size())
                    return std::unique_ptr<AbstractTunnelAcceptor>();

                auto acceptor = std::make_unique<udp::TunnelAcceptor>(
                    std::move(event.udpEndpointList.front()),
                    event.params);

                return std::unique_ptr<AbstractTunnelAcceptor>(std::move(acceptor));
            }

            return std::unique_ptr<AbstractTunnelAcceptor>();
        });

    // TODO: #mux add other connectors when supported
    return makers;
}

const std::vector<CloudServerSocket::AcceptorMaker>
    CloudServerSocket::kDefaultAcceptorMakers = defaultAcceptorMakers();

CloudServerSocket::CloudServerSocket(
    std::shared_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection,
    nx::network::RetryPolicy mediatorRegistrationRetryPolicy,
    std::vector<AcceptorMaker> acceptorMakers)
:
    m_mediatorConnection(std::move(mediatorConnection)),
    m_mediatorRegistrationRetryTimer(std::move(mediatorRegistrationRetryPolicy)),
    m_acceptorMakers(acceptorMakers),
    m_acceptQueueLen(kDefaultAcceptQueueSize),
    m_state(State::init)
{
    NX_ASSERT(m_mediatorConnection);

    // TODO: #mu default values for m_socketAttributes shall match default
    //           system vales: think how to implement this...
    m_socketAttributes.nonBlockingMode = false;
    m_socketAttributes.recvTimeout = 0;
}

CloudServerSocket::~CloudServerSocket()
{
    if (*m_socketAttributes.nonBlockingMode == false)
    {
        // Unfortunatelly we have to block here, cloud server socket uses
        // nonblocking operations even if user uses blocking mode.
        pleaseStopSync();
    }
    else
    if (m_mediatorConnection->inSelfAioThread())
    {
        // Will not block as all delegates are in the same AIO thread
        pleaseStopSync();
    }
    else
    {
        // pleaseStop(...) is expected to be called externally

        if (m_tunnelPool)
        {
            // It looksw like IO has been canceled, but it's not enought for
            // CloudServerSocket becaule of optimization.
            // It's better to block here, then end up with SIGSEGV isnt it?
            pleaseStopSync();
        }
    }
}

bool CloudServerSocket::bind(const SocketAddress& localAddress)
{
    // Does not make any sense in cloud socket context
    static_cast<void>(localAddress);
    return true;
}

SocketAddress CloudServerSocket::getLocalAddress() const
{
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
    //actual initialization is done with first accept call
    m_acceptQueueLen = queueLen != 0 ? queueLen : kDefaultAcceptQueueSize;
    return true;
}

AbstractStreamSocket* CloudServerSocket::accept()
{
    if (m_socketAttributes.nonBlockingMode && *m_socketAttributes.nonBlockingMode)
    {
        if (auto socket = m_tunnelPool->getNextSocketIfAny())
            return socket.release();

        SystemError::setLastErrorCode(SystemError::wouldBlock);
        return nullptr;
    }

    nx::utils::promise<SystemError::ErrorCode> promise;
    std::unique_ptr<AbstractStreamSocket> acceptedSocket;
    acceptAsync(
        [&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
        {
            acceptedSocket.reset(socket);
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

void CloudServerSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_mediatorRegistrationRetryTimer.pleaseStop(
        [this, handler = std::move(handler)]()
        {
            // shell not block as they are in the same AIO thread
            m_mediatorConnection->pleaseStopSync();
            m_acceptors.clear();
            m_tunnelPool.reset();
            handler();
        });
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
    const auto thread = m_mediatorRegistrationRetryTimer.getAioThread();
    m_mediatorConnection->bindToAioThread(thread);
    return thread;
}

void CloudServerSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    NX_ASSERT(!m_tunnelPool);
    m_mediatorRegistrationRetryTimer.bindToAioThread(aioThread);
    m_mediatorConnection->bindToAioThread(aioThread);
}

void CloudServerSocket::acceptAsync(
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode code,
        AbstractStreamSocket*)> handler)
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

                case State::readyToListen:
                    m_state = State::registeringOnMediator;
                    m_savedAcceptHandler = std::move(handler);
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
    m_mediatorRegistrationRetryTimer.post(  //doing post to avoid calling handler within this call
        [this, handler = std::move(handler)]() mutable
        {
            m_tunnelPool->cancelAccept();
            m_savedAcceptHandler = nullptr;
            handler();
        });
}

void CloudServerSocket::cancelIOSync()
{
    //we need dispatch here to avoid blocking if called within aio thread
    nx::utils::promise<void> cancelledPromise;
    m_mediatorRegistrationRetryTimer.dispatch(
        [this, &cancelledPromise]
        {
            //TODO #ak deal with copy-paste here
            m_tunnelPool->cancelAccept();
            m_savedAcceptHandler = nullptr;
            cancelledPromise.set_value();
        });
    cancelledPromise.get_future().wait();
}

void CloudServerSocket::registerOnMediator(
    nx::utils::MoveOnlyFunc<void(hpm::api::ResultCode)> handler)
{
    if (m_state == State::listening)
        return handler(hpm::api::ResultCode::ok);

    if (m_state == State::init)
    {
        initTunnelPool(m_acceptQueueLen);
        m_mediatorConnection->setOnConnectionRequestedHandler(std::bind(
            &CloudServerSocket::onConnectionRequested,
            this,
            std::placeholders::_1));
        m_state = State::readyToListen;
    }

    NX_ASSERT(m_state == State::readyToListen);
    m_state = State::registeringOnMediator;

    const auto cloudCredentials = m_mediatorConnection
        ->credentialsProvider()->getSystemCredentials();

    if (!cloudCredentials)
    {
        SystemError::setLastErrorCode(SystemError::invalidData);
        m_state = State::readyToListen;
        return handler(hpm::api::ResultCode::notAuthorized);
    }

    nx::hpm::api::ListenRequest listenRequestData;
    listenRequestData.systemId = cloudCredentials->systemId;
    listenRequestData.serverId = cloudCredentials->serverId;

    m_mediatorConnection->listen(
        std::move(listenRequestData),
        [this, handler = std::move(handler)](hpm::api::ResultCode code)
        {
            m_mediatorConnection->setOnReconnectedHandler(
                std::bind(&CloudServerSocket::onMediatorConnectionRestored, this));

            if (code == hpm::api::ResultCode::ok)
            {
                m_state = State::listening;
            }
            else
            {
                //TODO #ak set appropriate error code
                SystemError::setLastErrorCode(SystemError::invalidData);
                m_state = State::readyToListen;
            }

            handler(code);
        });
}

hpm::api::ResultCode CloudServerSocket::registerOnMediatorSync()
{
    nx::utils::promise<hpm::api::ResultCode> promise;
    registerOnMediator(
        [&promise](hpm::api::ResultCode code) { promise.set_value(code); });

    return promise.get_future().get();
}

void CloudServerSocket::moveToListeningState()
{
    if (!m_tunnelPool)
        initTunnelPool(m_acceptQueueLen);
    m_mediatorConnection->setOnConnectionRequestedHandler(
        std::bind(
            &CloudServerSocket::onConnectionRequested,
            this,
            std::placeholders::_1));
    m_state = State::listening;
}

void CloudServerSocket::initTunnelPool(int queueLen)
{
    m_tunnelPool = std::make_unique<IncomingTunnelPool>(
        getAioThread(), queueLen);
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
            NX_ASSERT(m_mediatorConnection->inSelfAioThread());
            NX_LOGX(lm("acceptor %1 returned %2: %3")
                    .arg(acceptorPtr).arg(connection)
                    .arg(SystemError::toString(code)), cl_logDEBUG2);

            const auto it = std::find_if(
                m_acceptors.begin(), m_acceptors.end(),
                [&](const std::unique_ptr<AbstractTunnelAcceptor>& a)
                { return a.get() == acceptorPtr; });

            NX_ASSERT(it != m_acceptors.end(), Q_FUNC_INFO, "where did it go?");
            m_acceptors.erase(it);

            if (code == SystemError::noError)
                m_tunnelPool->addNewTunnel(std::move(connection));
        });
}

void CloudServerSocket::onListenRequestCompleted(
    nx::hpm::api::ResultCode resultCode)
{
    NX_ASSERT(m_state == State::registeringOnMediator);
    if (resultCode == nx::hpm::api::ResultCode::ok)
    {
        m_state = State::listening;

        m_mediatorConnection->setOnReconnectedHandler(
            std::bind(&CloudServerSocket::onMediatorConnectionRestored, this));

        NX_LOGX(lm("Listen request completed successfully"), cl_logDEBUG2);
        auto acceptHandler = std::move(m_savedAcceptHandler);
        m_savedAcceptHandler = nullptr;
        if (acceptHandler)
            acceptAsyncInternal(std::move(acceptHandler));
    }
    else
    {
        //should retry if failed since system registration data
            //can be propagated to mediator with some delay
        if (m_mediatorRegistrationRetryTimer.scheduleNextTry(
                std::bind(&CloudServerSocket::issueRegistrationRequest, this)))
        {
            //another attempt will follow
            return;
        }

        m_state = State::readyToListen;

        NX_LOGX(lm("Listen request has failed: %1")
            .arg(QnLexical::serialized(resultCode)), cl_logINFO);
        auto acceptHandler = std::move(m_savedAcceptHandler);
        m_savedAcceptHandler = nullptr;
        if (acceptHandler)
            acceptHandler(SystemError::invalidData, nullptr);
    }
}

void CloudServerSocket::acceptAsyncInternal(
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode code,
        AbstractStreamSocket*)> handler)
{
    m_tunnelPool->getNextSocketAsync(
        [this, handler = std::move(handler)](
            std::unique_ptr<AbstractStreamSocket> socket) mutable
        {
            //NX_ASSERT(!m_acceptedSocket, Q_FUNC_INFO, "concurrently accepted socket");
            NX_LOGX(lm("accepted socket %1").arg(socket), cl_logDEBUG2);

            if (socket)
            {
                NX_LOGX(lm("return socket %1").arg(socket), cl_logDEBUG2);
                handler(SystemError::noError, socket.release());
            }
            else
            {
                NX_LOGX(lm("accept timed out"), cl_logDEBUG2);
                handler(SystemError::timedOut, nullptr);
            }
        },
        m_socketAttributes.recvTimeout);
}

void CloudServerSocket::issueRegistrationRequest()
{
    const auto cloudCredentials = m_mediatorConnection
        ->credentialsProvider()->getSystemCredentials();

    if (!cloudCredentials)  //TODO #ak this MUST be assert
    {
        //specially for unit tests
        m_mediatorRegistrationRetryTimer.dispatch(std::bind(
            &CloudServerSocket::onListenRequestCompleted, this,
            nx::hpm::api::ResultCode::notAuthorized));
        return;
    }

    nx::hpm::api::ListenRequest listenRequestData;
    listenRequestData.systemId = cloudCredentials->systemId;
    listenRequestData.serverId = cloudCredentials->serverId;
    m_mediatorConnection->listen(
        std::move(listenRequestData),
        [this](nx::hpm::api::ResultCode resultCode)
        {
            m_mediatorRegistrationRetryTimer.dispatch(std::bind(
                &CloudServerSocket::onListenRequestCompleted, this,
                resultCode));
        });
}

void CloudServerSocket::onConnectionRequested(
    hpm::api::ConnectionRequestedEvent event)
{
    m_mediatorRegistrationRetryTimer.dispatch(
        [this, event = std::move(event)]
        {
            for (const auto& maker : m_acceptorMakers)
            {
                if (auto acceptor = maker(event))
                {
                    acceptor->setConnectionInfo(
                        event.connectSessionId, event.originatingPeerID);

                    acceptor->setMediatorConnection(m_mediatorConnection);
                    startAcceptor(std::move(acceptor));
                }
            }

            if (event.connectionMethods)
                NX_LOGX(lm("Unsupported ConnectionMethods: %1")
                    .arg(event.connectionMethods), cl_logWARNING);
        });
}

void CloudServerSocket::onMediatorConnectionRestored()
{
    //TODO #ak it's a pity we cannot move m_mediatorConnection to this object's aio thread
    m_mediatorRegistrationRetryTimer.dispatch(  //modifiyng state only in aio thread
        [this]
        {
            NX_LOGX(lm("Connection to mediator has been restored after failure. "
                "Re-sending listen request"), cl_logDEBUG1);

            if (m_state == State::listening)
            {
                m_state = State::registeringOnMediator;
                //sending listen request again
                m_mediatorRegistrationRetryTimer.reset();
                issueRegistrationRequest();
            }
        });
}

} // namespace cloud
} // namespace network
} // namespace nx
