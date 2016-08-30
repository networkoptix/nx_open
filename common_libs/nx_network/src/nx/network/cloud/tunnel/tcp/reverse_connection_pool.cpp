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

    const auto connections = holder->connectionsCount();
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
    m_aioThread(aioThread)
{
}

size_t ReverseConnectionHolder::connectionsCount() const
{
    QnMutexLocker lk(&m_mutex);
    return m_sockets.size();
}

void ReverseConnectionHolder::takeConnection(
    std::function<void(std::unique_ptr<AbstractStreamSocket>)> handler)
{
    QnMutexLocker lk(&m_mutex);
    NX_CRITICAL(!m_handler, "Multiple handlers are not supported!");
    m_handler = std::move(handler);

    if (m_sockets.empty())
        return;

    auto socket = std::move(m_sockets.front());
    m_sockets.pop_front();
    lk.unlock();

    auto socketPtr = socket.get();
    socketPtr->cancelIOAsync(
        aio::etNone,
        [this, socket = std::move(socket)]() mutable
        {
            QnMutexLocker lk(&m_mutex);
            m_handler(std::move(socket));
            m_handler = nullptr;
        });
}

void ReverseConnectionHolder::clearConnectionHandler()
{
    QnMutexLocker lk(&m_mutex);
    m_handler = nullptr;
}

void ReverseConnectionHolder::newSocket(std::unique_ptr<AbstractStreamSocket> socket)
{
    QnMutexLocker lk(&m_mutex);
    if (m_handler)
    {
        m_handler(std::move(socket));
        m_handler = nullptr;
        return;
    }

    const auto it = m_sockets.insert(m_sockets.end(), std::move(socket));
    (*it)->bindToAioThread(m_aioThread);
    monitorSocket(it);
}

void ReverseConnectionHolder::monitorSocket(std::list<std::unique_ptr<AbstractStreamSocket>>::iterator it)
{
    // NOTE: Here we monitor for connection close by recv as no any data is expected
    // to pop up from the socket (in such case socket will be closed anyway)
    auto buffer = std::make_shared<Buffer>();
    buffer->reserve(100);
    (*it)->readSomeAsync(
        buffer.get(),
        [this, it, buffer](SystemError::ErrorCode code, size_t size)
        {
            QnMutexLocker lk(&m_mutex);
            if (code == SystemError::timedOut)
                return monitorSocket(it);

            NX_LOGX(lm("Unexpected event on socket(%1), size=%2: %3")
                .args(*it, size, SystemError::toString(code)),
                    size ? cl_logERROR : cl_logDEBUG1);

            (void)buffer; //< This buffer might be helpful for debug is case smth goes wrong!
            m_sockets.erase(it);
        });
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
