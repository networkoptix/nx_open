// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "outgoing_tunnel.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/barrier_handler.h>

#include "connector_factory.h"
#include "../cloud_connect_settings.h"

namespace nx::network::cloud {

constexpr std::chrono::milliseconds AbstractOutgoingTunnel::kNoTimeout;

OutgoingTunnel::OutgoingTunnel(
    const CloudConnectSettings& settings,
    AddressEntry targetPeerAddress)
    :
    m_tunnelId(QnUuid::createUuid().toSimpleByteArray().toStdString()),
    m_settings(settings),
    m_targetPeerAddress(std::move(targetPeerAddress)),
    m_timer(std::make_unique<aio::Timer>())
{
    m_timer->bindToAioThread(getAioThread());

    NX_VERBOSE(this, "Created tunnel %1, target address %2", m_tunnelId, m_targetPeerAddress);
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
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(!m_onClosedHandler, "State handler is already set");
    m_onClosedHandler = std::move(handler);
}

void OutgoingTunnel::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    NewConnectionHandler handler)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    NX_ASSERT(!m_terminated);

    NX_VERBOSE(this, "%1. New connection requested while in state %2",
        m_tunnelId, toString(m_state));

    switch (m_state)
    {
        case State::connected:
            lk.unlock();

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
                    NX_MUTEX_LOCKER lk(&m_mutex);
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
            NX_ASSERT(false, nx::format("Unexpected state %1").arg(toString(m_state)));
            break;
    }
}

std::string OutgoingTunnel::toString(State state)
{
    switch (state)
    {
        case State::init:
            return "init";
        case State::connecting:
            return "connecting";
        case State::connected:
            return "connected";
        case State::closed:
            return "closed";
    }

    return nx::utils::buildString("unknown(", std::to_string((int) state), ')');
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
    NX_MUTEX_LOCKER lk(&m_mutex);
    updateTimerIfNeededNonSafe(&lk, std::chrono::steady_clock::now());
}

void OutgoingTunnel::updateTimerIfNeededNonSafe(
    nx::Locker<nx::Mutex>* const /*lock*/,
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

    NX_MUTEX_LOCKER lk(&m_mutex);
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
    NX_VERBOSE(this, "%1. Connect completed. Result %2, still valid: %3",
        m_tunnelId, SystemError::toString(code), stillValid);

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
                NX_MUTEX_LOCKER lk(&m_mutex);
                std::swap(tunnelClosedHandler, m_onClosedHandler);
                m_state = State::closed;
                m_lastErrorCode = errorCode;
            }
            if (tunnelClosedHandler)
                tunnelClosedHandler();
        });
}

void OutgoingTunnel::startAsyncTunnelConnect(nx::Locker<nx::Mutex>* const /*locker*/)
{
    m_state = State::connecting;
    m_connector = CrossNatConnectorFactory::instance().create(
        m_tunnelId,
        m_targetPeerAddress);
    m_connector->bindToAioThread(getAioThread());
    m_connector->connect(
        m_settings.cloudConnectTimeout,
        [this](auto&&... args) { return onConnectorFinished(std::forward<decltype(args)>(args)...); });
}

void OutgoingTunnel::onConnectorFinished(
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
{
    NX_VERBOSE(this, "%1. Connector completed with result %2",
        m_tunnelId, SystemError::toString(errorCode));

    NX_MUTEX_LOCKER lk(&m_mutex);

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
    NX_VERBOSE(this, "%1. Tunnel connection obtained. There are %2 pending connection(s)",
        m_tunnelId, m_connectHandlers.size());

    m_connection = std::move(connection);
    m_connection->setControlConnectionClosedHandler(
        [this](auto reason) { onTunnelClosed(reason); });
    m_connection->start();
    m_state = State::connected;

    NX_ASSERT(m_connection->getAioThread() == getAioThread());

    // We've connected to the host. Requesting client connections.
    for (auto& connectRequest: m_connectHandlers)
    {
        m_connection->establishNewConnection(
            connectRequest.second.timeout,   //< TODO #akolesnikov recalculate timeout.
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
    base_type([this](auto&&... args) {
        return defaultFactoryFunction(std::forward<decltype(args)>(args)...);
    })
{
}

OutgoingTunnelFactory& OutgoingTunnelFactory::instance()
{
    static OutgoingTunnelFactory instance;
    return instance;
}

std::unique_ptr<AbstractOutgoingTunnel> OutgoingTunnelFactory::defaultFactoryFunction(
    const CloudConnectSettings& settings,
    const AddressEntry& address)
{
    return std::make_unique<OutgoingTunnel>(settings, address);
}

} // namespace nx::network::cloud
