#include "outgoing_tunnel.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/barrier_handler.h>

#include "connector_factory.h"

namespace nx {
namespace network {
namespace cloud {

const std::chrono::seconds kCloudConnectorTimeout(10);

constexpr std::chrono::milliseconds AbstractOutgoingTunnel::kNoTimeout;

OutgoingTunnel::OutgoingTunnel(AddressEntry targetPeerAddress):
    m_targetPeerAddress(std::move(targetPeerAddress)),
    m_timer(std::make_unique<aio::Timer>()),
    m_terminated(false),
    m_lastErrorCode(SystemError::noError),
    m_state(State::init)
{
    m_timer->bindToAioThread(getAioThread());
}

OutgoingTunnel::~OutgoingTunnel()
{
    stopWhileInAioThread();
}

void OutgoingTunnel::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    if (m_timer)
        m_timer->bindToAioThread(aioThread);
    if (m_connector)
        m_connector->bindToAioThread(aioThread);
    if (m_connection)
        m_connection->bindToAioThread(aioThread);
}

void OutgoingTunnel::stopWhileInAioThread()
{
    //do not need to lock mutex since it is unexpected if
    //  someone calls public methods while stopping object

    m_terminated = true;
    m_connector.reset();
    m_connection.reset();
    m_timer.reset();
    m_connectHandlers.clear();
}

void OutgoingTunnel::setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler)
{
    QnMutexLocker lock(&m_mutex);
    NX_ASSERT(!m_onClosedHandler, Q_FUNC_INFO, "State handler is already set");
    m_onClosedHandler = std::move(handler);
}

void OutgoingTunnel::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    NewConnectionHandler handler)
{
    QnMutexLocker lk(&m_mutex);
    NX_ASSERT(!m_terminated);

    switch (m_state)
    {
        case State::connected:
            lk.unlock();
            using namespace std::placeholders;
            m_connection->establishNewConnection(
                timeout,
                std::move(socketAttributes),
                [handler = std::move(handler), this](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> socket,
                    bool tunnelStillValid) mutable
                {
                    onConnectFinished(
                        std::move(handler),
                        errorCode,
                        std::move(socket),
                        tunnelStillValid);
                });
            break;

        case State::closed:
        {
            // Just saving handler to notify it before tunnel destruction.
            ConnectionRequestData data;
            data.socketAttributes = std::move(socketAttributes);
            data.handler = std::move(handler);
            auto handlerIter = m_connectHandlers.emplace(
                std::chrono::steady_clock::time_point::max(),
                std::move(data));
            lk.unlock();
            post(
                [handlerIter, this]
                {
                    handlerIter->second.handler(m_lastErrorCode, TunnelAttributes(), nullptr);
                    QnMutexLocker lk(&m_mutex);
                    m_connectHandlers.erase(handlerIter);
                });
            break;
        }

        case State::init:
            startAsyncTunnelConnect(&lk);
            postponeConnectTask(timeout, std::move(socketAttributes), std::move(handler));
            break;

        case State::connecting:
            postponeConnectTask(timeout, std::move(socketAttributes), std::move(handler));
            break;

        default:
            NX_ASSERT(
                false,
                Q_FUNC_INFO,
                lm("Unexpected state %1")
                    .arg(stateToString(m_state)).toStdString().c_str());
            break;
    }
}

QString OutgoingTunnel::stateToString(State state)
{
    switch (state)
    {
        case State::init:
            return lm("init");
        case State::connecting:
            return lm("connecting");
        case State::connected:
            return lm("connected");
        case State::closed:
            return lm("closed");
    }

    return lm("unknown(%1)").arg(static_cast<int>(state));
}

void OutgoingTunnel::postponeConnectTask(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    NewConnectionHandler handler)
{
    const auto timeoutTimePoint =
        timeout > std::chrono::milliseconds::zero()
            ? std::chrono::steady_clock::now() + timeout
            : std::chrono::steady_clock::time_point::max();
    ConnectionRequestData data;
    data.socketAttributes = std::move(socketAttributes);
    data.timeout = timeout;
    data.handler = std::move(handler);
    m_connectHandlers.emplace(timeoutTimePoint, std::move(data));

    post(std::bind(&OutgoingTunnel::updateTimerIfNeeded, this));
}

void OutgoingTunnel::updateTimerIfNeeded()
{
    QnMutexLocker lk(&m_mutex);
    updateTimerIfNeededNonSafe(&lk, std::chrono::steady_clock::now());
}

void OutgoingTunnel::updateTimerIfNeededNonSafe(
    QnMutexLockerBase* const /*lock*/,
    const std::chrono::steady_clock::time_point curTime)
{
    if (!m_connectHandlers.empty() &&
        (!m_timerTargetClock || *m_timerTargetClock > m_connectHandlers.begin()->first))
    {
        //cancelling current timer
        m_timer->cancelSync();

        //starting new timer
        m_timerTargetClock = m_connectHandlers.begin()->first;
        const auto timeout =
            m_connectHandlers.begin()->first > curTime
            ? (m_connectHandlers.begin()->first - curTime)
            : std::chrono::milliseconds::zero();    //timeout has already expired
        m_timer->start(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout),
            std::bind(&OutgoingTunnel::onTimer, this));
    }
}

void OutgoingTunnel::onTimer()
{
    const auto curTime = std::chrono::steady_clock::now();
    std::vector<ConnectionRequestData> timedoutConnectOperations;

    QnMutexLocker lk(&m_mutex);
    for (auto it = m_connectHandlers.begin(); it != m_connectHandlers.end();)
    {
        using namespace std::chrono;
        //resolution of timer is millisecond and zero timeout is not supported
        if ((it->first > curTime) &&
            (duration_cast<milliseconds>(it->first - curTime) > milliseconds(0)))
        {
            break;
        }
        //operation timedout
        timedoutConnectOperations.emplace_back(std::move(it->second));
        m_connectHandlers.erase(it++);
    }
    m_timerTargetClock.reset();
    updateTimerIfNeededNonSafe(&lk, curTime);

    lk.unlock();
    //triggering timedout operations
    for (const auto& connectOperationContext: timedoutConnectOperations)
    {
        connectOperationContext.handler(
            SystemError::timedOut,
            TunnelAttributes(),
            std::unique_ptr<AbstractStreamSocket>());
    }
}

void OutgoingTunnel::onConnectFinished(
    NewConnectionHandler handler,
    SystemError::ErrorCode code,
    std::unique_ptr<AbstractStreamSocket> socket,
    bool stillValid)
{
    handler(code, m_attributes, std::move(socket));
    if (code != SystemError::noError || !stillValid)
        onTunnelClosed(stillValid ? code : SystemError::connectionReset);
}

void OutgoingTunnel::onTunnelClosed(SystemError::ErrorCode errorCode)
{
    dispatch(
        [this, errorCode]()
        {
            nx::utils::MoveOnlyFunc<void()> tunnelClosedHandler;
            {
                QnMutexLocker lk(&m_mutex);
                std::swap(tunnelClosedHandler, m_onClosedHandler);
                m_state = State::closed;
                m_lastErrorCode = errorCode;
            }
            if (tunnelClosedHandler)
                tunnelClosedHandler();
        });
}

void OutgoingTunnel::startAsyncTunnelConnect(QnMutexLockerBase* const /*locker*/)
{
    using namespace std::placeholders;

    m_state = State::connecting;
    m_connector = CrossNatConnectorFactory::instance().create(m_targetPeerAddress);
    m_connector->bindToAioThread(getAioThread());
    m_connector->connect(
        kCloudConnectorTimeout,
        std::bind(&OutgoingTunnel::onConnectorFinished, this, _1, _2));
}

void OutgoingTunnel::onConnectorFinished(
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
{
    QnMutexLocker lk(&m_mutex);

    m_attributes.remotePeerName = m_connector->getRemotePeerName();

    NX_ASSERT(!m_connection);
    m_connector.reset();

    if (errorCode == SystemError::noError)
    {
        setTunnelConnection(std::move(connection));
        return;
    }

    // Reporting error to everyone who is waiting.
    decltype(m_connectHandlers) connectHandlers;
    connectHandlers.swap(m_connectHandlers);
    m_state = State::closed;
    m_lastErrorCode = errorCode;
    lk.unlock();
    for (auto& connectRequest: connectHandlers)
        connectRequest.second.handler(errorCode, TunnelAttributes(), nullptr);
    // Reporting tunnel failure.
    onTunnelClosed(errorCode);
}

void OutgoingTunnel::setTunnelConnection(
    std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
{
    m_connection = std::move(connection);
    m_connection->setControlConnectionClosedHandler(
        std::bind(&OutgoingTunnel::onTunnelClosed, this, std::placeholders::_1));
    m_connection->start();
    m_state = State::connected;

    NX_ASSERT(m_connection->getAioThread() == getAioThread());

    // We've connected to the host. Requesting client connections.
    for (auto& connectRequest: m_connectHandlers)
    {
        using namespace std::placeholders;
        m_connection->establishNewConnection(
            connectRequest.second.timeout,   //< TODO #ak recalculate timeout.
            std::move(connectRequest.second.socketAttributes),
            [handler = std::move(connectRequest.second.handler), this](
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractStreamSocket> socket,
                bool tunnelStillValid) mutable
            {
                onConnectFinished(
                    std::move(handler),
                    errorCode,
                    std::move(socket),
                    tunnelStillValid);
            });
    }
    m_connectHandlers.clear();
}

//-------------------------------------------------------------------------------------------------

OutgoingTunnelFactory::OutgoingTunnelFactory():
    base_type(std::bind(&OutgoingTunnelFactory::defaultFactoryFunction,
        this, std::placeholders::_1))
{
}

OutgoingTunnelFactory& OutgoingTunnelFactory::instance()
{
    static OutgoingTunnelFactory instance;
    return instance;
}

std::unique_ptr<AbstractOutgoingTunnel> OutgoingTunnelFactory::defaultFactoryFunction(
    const AddressEntry& address)
{
    return std::make_unique<OutgoingTunnel>(address);
}

} // namespace cloud
} // namespace network
} // namespace nx
