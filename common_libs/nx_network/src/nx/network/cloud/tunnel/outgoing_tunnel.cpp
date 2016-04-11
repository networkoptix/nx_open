/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "outgoing_tunnel.h"

#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>

#include "connector_factory.h"


namespace nx {
namespace network {
namespace cloud {

const std::chrono::seconds kCloudConnectorTimeout(10);

OutgoingTunnel::OutgoingTunnel(AddressEntry targetPeerAddress)
:
    Tunnel(targetPeerAddress.host.toString().toLatin1()),
    m_targetPeerAddress(std::move(targetPeerAddress)),
    m_terminated(false),
    m_counter(0),
    m_lastErrorCode(SystemError::noError)
{
    m_timer.getAioThread();   //binds to aio thread
}

OutgoingTunnel::~OutgoingTunnel()
{
    NX_ASSERT(m_connectors.empty());
    if (!m_terminated)
    {
        //in this case there MUST be tunnelClosed handler down the stack
        NX_ASSERT(m_state == State::kClosed);

        for (auto& connectRequest : m_connectHandlers)
            connectRequest.second.handler(SystemError::interrupted, nullptr);
        m_connectHandlers.clear();
    }
    else
    {
        NX_ASSERT(m_connectHandlers.empty());
    }
}

void OutgoingTunnel::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    QnMutexLocker lk(&m_mutex);

    m_terminated = true;

    if (!m_connectors.empty())
    {
        BarrierHandler barrier(
            [this, handler = std::move(handler)]() mutable
            {
                m_timer.post(
                    [this, handler = std::move(handler)]() mutable
                    {
                        connectorsTerminated(std::move(handler));
                    });
            });
        for (const auto& connectorData: m_connectors)
            connectorData.second->pleaseStop(barrier.fork());
    }
    else
    {
        connectorsTerminatedNonSafe(&lk, std::move(handler));
    }
}

void OutgoingTunnel::connectorsTerminated(
    nx::utils::MoveOnlyFunc<void()> pleaseStopCompletionHandler)
{
    QnMutexLocker lk(&m_mutex);
    connectorsTerminatedNonSafe(&lk, std::move(pleaseStopCompletionHandler));
}

void OutgoingTunnel::connectorsTerminatedNonSafe(
    QnMutexLockerBase* const /*lock*/,
    nx::utils::MoveOnlyFunc<void()> pleaseStopCompletionHandler)
{
    m_connectors.clear();

    //cancelling connection
    if (m_connection)
        m_connection->pleaseStop(
            [this, handler = std::move(pleaseStopCompletionHandler)]() mutable
            {
                connectionTerminated(std::move(handler));
            });
    else
        connectionTerminated(std::move(pleaseStopCompletionHandler));
}

void OutgoingTunnel::connectionTerminated(
    nx::utils::MoveOnlyFunc<void()> pleaseStopCompletionHandler)
{
    m_timer.post(
        [this, handler = std::move(pleaseStopCompletionHandler)]() mutable
        {
            m_timer.pleaseStopSync();
            {
                //waiting for OutgoingTunnel::pleaseStop still running in another thread to return
                QnMutexLocker lk(&m_mutex);
            }
            for (auto& connectRequest : m_connectHandlers)
                connectRequest.second.handler(SystemError::interrupted, nullptr);
            m_connectHandlers.clear();
            handler();
        });
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
        case State::kConnected:
            lk.unlock();
            using namespace std::placeholders;
            m_connection->establishNewConnection(
                timeout,
                std::move(socketAttributes),
                [handler = std::move(handler), this](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> socket,
                    bool tunnelStillValid)
                {
                    onConnectFinished(
                        std::move(handler),
                        errorCode,
                        std::move(socket),
                        tunnelStillValid);
                });
            break;

        case State::kClosed:
        {
            //just saving handler to notify it before tunnel destruction
            ConnectionRequestData data;
            data.socketAttributes = std::move(socketAttributes);
            data.handler = std::move(handler);
            auto handlerIter = m_connectHandlers.emplace(
                std::chrono::steady_clock::time_point::max(),
                std::move(data));
            lk.unlock();
            m_timer.post(
                [handlerIter, this]
                {
                    handlerIter->second.handler(m_lastErrorCode, nullptr);
                    QnMutexLocker lk(&m_mutex);
                    m_connectHandlers.erase(handlerIter);
                });
            break;
        }

        case State::kInit:
            startAsyncTunnelConnect(&lk);

        case State::kConnecting:
        {
            //saving handler for later use
            const auto timeoutTimePoint =
                timeout > std::chrono::milliseconds::zero()
                ? std::chrono::steady_clock::now() + timeout
                : std::chrono::steady_clock::time_point::max();
            ConnectionRequestData data;
            data.socketAttributes = std::move(socketAttributes);
            data.timeout = timeout;
            data.handler = std::move(handler);
            m_connectHandlers.emplace(timeoutTimePoint, std::move(data));

            m_timer.post(std::bind(&OutgoingTunnel::updateTimerIfNeeded, this));
            break;
        }

        default:
            NX_ASSERT(
                false,
                Q_FUNC_INFO,
                lm("Unexpected state %1")
                    .arg(stateToString(m_state)).toStdString().c_str());
            break;
    }
}

void OutgoingTunnel::establishNewConnection(
    SocketAttributes socketAttributes,
    NewConnectionHandler handler)
{
    establishNewConnection(
        std::chrono::milliseconds::zero(),
        std::move(socketAttributes),
        std::move(handler));
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
        m_timer.cancelSync();

        //starting new timer
        m_timerTargetClock = m_connectHandlers.begin()->first;
        const auto timeout =
            m_connectHandlers.begin()->first > curTime
            ? (m_connectHandlers.begin()->first - curTime)
            : std::chrono::milliseconds::zero();    //timeout has already expired
        m_timer.start(
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
        connectOperationContext.handler(
            SystemError::timedOut,
            std::unique_ptr<AbstractStreamSocket>());
}

void OutgoingTunnel::onConnectFinished(
    NewConnectionHandler handler,
    SystemError::ErrorCode code,
    std::unique_ptr<AbstractStreamSocket> socket,
    bool stillValid)
{
    handler(code, std::move(socket));
    if (code != SystemError::noError || !stillValid)
        onTunnelClosed(stillValid ? code : SystemError::connectionReset);
}

void OutgoingTunnel::onTunnelClosed(SystemError::ErrorCode errorCode)
{
    m_timer.dispatch(
        [this, errorCode]()
        {
            std::function<void(State)> tunnelClosedHandler;
            {
                QnMutexLocker lk(&m_mutex);
                tunnelClosedHandler = std::move(m_stateHandler);
                m_state = State::kClosed;
                m_lastErrorCode = errorCode;
            }
            if (tunnelClosedHandler)
                tunnelClosedHandler(State::kClosed);
        });
}

void OutgoingTunnel::startAsyncTunnelConnect(QnMutexLockerBase* const /*locker*/)
{
    m_state = State::kConnecting;
    m_connectors = ConnectorFactory::createAllCloudConnectors(m_targetPeerAddress);
    for (auto& connector: m_connectors)
    {
        auto connectorType = connector.first;
        connector.second->bindToAioThread(m_timer.getAioThread());
        connector.second->connect(
            kCloudConnectorTimeout,
            [connectorType, this](
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
            {
                //m_timer.post(
                onConnectorFinished(
                    connectorType,
                    errorCode,
                    std::move(connection));
            });
    }
}

void OutgoingTunnel::onConnectorFinished(
    CloudConnectType connectorType,
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
{
    QnMutexLocker lk(&m_mutex);

    if (m_terminated)
        return; //we must not use m_connection anymore

    const auto connectorIter = m_connectors.find(connectorType);
    if (connectorIter == m_connectors.end())
        return; //it can happen when stopping OutgoingTunnel
    auto connector = std::move(connectorIter->second);
    m_connectors.erase(connectorIter);

    if (errorCode == SystemError::noError)
    {
        if (m_connection)
            return; //tunnel has already been connected, just ignoring this connection

        m_connection = std::move(connection);
        m_state = State::kConnected;
        //we've connected to the host. Requesting client connections
        for (auto& connectRequest: m_connectHandlers)
        {
            using namespace std::placeholders;
            m_connection->establishNewConnection(
                connectRequest.second.timeout,   //TODO #ak recalculate timeout
                std::move(connectRequest.second.socketAttributes),
                [handler = std::move(connectRequest.second.handler), this](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> socket,
                    bool tunnelStillValid)
                {
                    onConnectFinished(
                        std::move(handler),
                        errorCode,
                        std::move(socket),
                        tunnelStillValid);
                });
        }

        m_connectHandlers.clear();
        return;
    }

    //connection failed
    if (!m_connectors.empty())
        return; //waiting for other connectors to complete

    //reporting error to everyone who is waiting
    auto connectHandlers = std::move(m_connectHandlers);
    m_state = State::kClosed;
    m_lastErrorCode = errorCode;
    lk.unlock();
    for (auto& connectRequest : connectHandlers)
        connectRequest.second.handler(errorCode, nullptr);
    //reporting tunnel failure
    onTunnelClosed(errorCode);
}

} // namespace cloud
} // namespace network
} // namespace nx
