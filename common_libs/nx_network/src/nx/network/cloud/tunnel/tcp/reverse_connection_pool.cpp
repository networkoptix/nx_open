#include "reverse_connection_pool.h"

#include <nx/network/cloud/data/client_bind_data.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

ReverseConnectionPool::ReverseConnectionPool(
    std::shared_ptr<MediatorConnection> mediatorConnection)
:
    m_mediatorConnection(std::move(mediatorConnection))
{
}

bool ReverseConnectionPool::start(const SocketAddress& address, bool waitForRegistration)
{
    NX_ASSERT(!m_acceptor);
    m_acceptor = std::make_unique<ReverseAcceptor>(
        QnUuid::createUuid().toByteArray(),
        [this](String hostName, std::unique_ptr<AbstractStreamSocket> socket)
        {
            NX_LOGX(lm("New socket(%1) from '%2").args(socket, hostName), cl_logDEBUG1);
            getHolder(hostName, true)->newSocket(std::move(socket));
        });

    if (!m_acceptor->start(address, m_mediatorConnection->getAioThread()))
    {
        NX_LOGX(lm("Could not start acceptor: %1").arg(SystemError::getLastOSErrorText()),
            cl_logWARNING);

        return false;
    }

    return registerOnMediator(waitForRegistration);
}

std::shared_ptr<ReverseConnectionHolder>
    ReverseConnectionPool::getConnectionHolder(const String& hostName)
{
    auto holder = getHolder(hostName, false);
    if (!holder)
        return nullptr;

    const auto connections = holder->socketCount();
    if (connections == 0)
        return nullptr;

    NX_LOGX(lm("Returning holder for '%1' with %2 connections(s)")
        .strs(hostName, connections), cl_logDEBUG1);

    return holder;
}

void ReverseConnectionPool::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_mediatorConnection->pleaseStop(
        [this, handler = std::move(completionHandler)]()
        {
            m_acceptor.reset();
            for (auto& holder: m_connectionHolders)
                holder.second->stopInAioThread();

            m_connectionHolders.clear();
            handler();
        });
}

void ReverseConnectionPool::setPoolSize(boost::optional<size_t> value)
{
    m_acceptor->setPoolSize(std::move(value));

    // TODO: also need to close extra connections in m_connectionHolders
}

void ReverseConnectionPool::setKeepAliveOptions(boost::optional<KeepAliveOptions> value)
{
    m_acceptor->setKeepAliveOptions(std::move(value));
}

bool ReverseConnectionPool::registerOnMediator(bool waitForRegistration)
{
    std::shared_ptr<utils::promise<bool>> registrationPromise;
    if (waitForRegistration)
        registrationPromise = std::make_shared<utils::promise<bool>>();

    hpm::api::ClientBindRequest request;
    request.originatingPeerID = m_acceptor->selfHostName();
    request.tcpReverseEndpoints.push_back(m_acceptor->address());
    m_mediatorConnection->send(
        std::move(request),
        [this, registrationPromise](nx::hpm::api::ResultCode code)
        {
            if (code == nx::hpm::api::ResultCode::ok)
            {
                NX_LOGX(lm("Registred on mediator by %1 with %2")
                    .strs(m_acceptor->selfHostName(), m_acceptor->address()), cl_logINFO);
            }
            else
            {
                NX_LOGX(lm("Could not register on mediator: %1").str(code), cl_logWARNING);
            }

            // Reregister on reconnect:
            m_mediatorConnection->setOnReconnectedHandler([this](){ registerOnMediator(); });

            if (registrationPromise)
                registrationPromise->set_value(code == nx::hpm::api::ResultCode::ok);
        });

    if (registrationPromise)
        return registrationPromise->get_future().get();

    return true;
}

std::shared_ptr<ReverseConnectionHolder>
    ReverseConnectionPool::getHolder(const String& hostName, bool mayCreate)
{
    QnMutexLocker lk(&m_mutex);
    auto it = m_connectionHolders.find(hostName);
    if (it == m_connectionHolders.end())
    {
        if (!mayCreate)
            return nullptr;

        it = m_connectionHolders.emplace(
            std::move(hostName),
            std::make_shared<ReverseConnectionHolder>(
                m_mediatorConnection->getAioThread())).first;

        // Register the same holder for system if it was not registred yet:
        const auto split = hostName.split('.');
        if (split.size() == 2)
        {
            auto& systemId = split.last();
            if (m_connectionHolders.find(systemId) == m_connectionHolders.end())
                m_connectionHolders.emplace(std::move(systemId), it->second);
        }
    }

    return it->second;
}

ReverseConnectionHolder::ReverseConnectionHolder(aio::AbstractAioThread* aioThread):
    m_socketCount(0)
{
    m_timer.bindToAioThread(aioThread);
}


void ReverseConnectionHolder::stopInAioThread()
{
    m_timer.pleaseStopSync();
}

void ReverseConnectionHolder::newSocket(std::unique_ptr<AbstractStreamSocket> socket)
{
    if (!m_handlers.empty())
    {
        auto handler = std::move(m_handlers.begin()->second);
        m_handlers.erase(m_handlers.begin());
        return handler(SystemError::noError, std::move(socket));
    }

    const auto it = m_sockets.insert(m_sockets.end(), std::move(socket));
    ++m_socketCount;
    (*it)->bindToAioThread(m_timer.getAioThread());
    monitorSocket(it);
}

size_t ReverseConnectionHolder::socketCount() const
{
    return m_socketCount;
}

void ReverseConnectionHolder::takeSocket(std::chrono::milliseconds timeout, Handler handler)
{
    m_timer.post(
        [this, expirationTime = std::chrono::steady_clock::now() + timeout,
            handler = std::move(handler)]() mutable
        {
            if (m_sockets.size())
            {
                auto socket = std::move(m_sockets.front());
                m_sockets.pop_front();
                --m_socketCount;

                socket->cancelIOSync(aio::etNone);
                handler(SystemError::noError, std::move(socket));
            }
            else
            {
                const auto timeLeft = std::chrono::duration_cast<std::chrono::milliseconds>(
                    expirationTime - std::chrono::steady_clock::now());

                if (timeLeft.count() <= 0)
                    return handler(SystemError::timedOut, nullptr);

                if (m_handlers.empty() || expirationTime < m_handlers.begin()->first)
                {
                    m_timer.start(
                        timeLeft,
                        [this]()
                        {
                            const auto now = std::chrono::steady_clock::now();
                            for (auto it = m_handlers.begin(); it != m_handlers.end(); )
                            {
                                if (it->first > now)
                                    return;

                                it->second(SystemError::timedOut, nullptr);
                                it = m_handlers.erase(it);
                            }
                        });
                }

                m_handlers.emplace(expirationTime, std::move(handler));
            }
        });
}

void ReverseConnectionHolder::monitorSocket(
    std::list<std::unique_ptr<AbstractStreamSocket>>::iterator it)
{
    // NOTE: Here we monitor for connection close by recv as no any data is expected
    // to pop up from the socket (in such case socket will be closed anyway)
    auto buffer = std::make_shared<Buffer>();
    buffer->reserve(100);
    (*it)->readSomeAsync(
        buffer.get(),
        [this, it, buffer](SystemError::ErrorCode code, size_t size)
        {
            if (code == SystemError::timedOut)
                return monitorSocket(it);

            NX_LOGX(lm("Unexpected event on socket(%1), size=%2: %3")
                .args(*it, size, SystemError::toString(code)),
                    size ? cl_logERROR : cl_logDEBUG1);

            (void)buffer; //< This buffer might be helpful for debug is case smth goes wrong!
            --m_socketCount;
            m_sockets.erase(it);
        });
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
