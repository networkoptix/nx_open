#include "reverse_connection_pool.h"

#include <nx/network/cloud/data/client_bind_data.h>

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

bool ReverseConnectionPool::start(const SocketAddress& address)
{
    NX_ASSERT(!m_acceptor);
    m_acceptor = std::make_unique<ReverseAcceptor>(
        QnUuid::createUuid().toByteArray(),
        [this](String hostName, std::unique_ptr<AbstractStreamSocket> socket)
        {
            NX_LOGX(lm("New socket(%1) from '%2").args(socket, hostName), cl_logDEBUG1);
            getConnectionHolder(hostName)->newSocket(std::move(socket));
        });

    if (!m_acceptor->start(address, m_mediatorConnection->getAioThread()))
    {
        NX_LOGX(lm("Could not start acceptor: %1").arg(SystemError::getLastOSErrorText()),
            cl_logWARNING);

        return false;
    }

    m_mediatorConnection->setOnReconnectedHandler(
        [this]()
        {
            hpm::api::ClientBindRequest request;
            request.originatingPeerID = m_acceptor->selfHostName();
            request.tcpReverseEndpoints.push_back(m_acceptor->address());
            m_mediatorConnection->send(
                std::move(request),
                [this](nx::hpm::api::ResultCode code)
                {
                    if (code != nx::hpm::api::ResultCode::ok)
                    {
                        NX_LOGX(lm("Could not register on mediator: %1").str(code), cl_logWARNING);
                        return;
                    }

                    NX_LOGX(lm("Registred on mediator by %1 with %2")
                        .strs(m_acceptor->selfHostName(), m_acceptor->address()), cl_logINFO);
                });
        });

    return true;
}

std::shared_ptr<ReverseConnectionHolder>
    ReverseConnectionPool::getConnectionHolder(const String& hostName)
{
    QnMutexLocker lk(&m_mutex);
    return m_connectionHolders[hostName];
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
}

void ReverseConnectionPool::setKeepAliveOptions(boost::optional<KeepAliveOptions> value)
{
    m_acceptor->setKeepAliveOptions(std::move(value));
}

ReverseConnectionHolder::ReverseConnectionHolder(aio::AbstractAioThread* aioThread):
    m_aioThread(aioThread)
{
}

bool ReverseConnectionHolder::hasConnections() const
{
    QnMutexLocker lk(&m_mutex);
    return !m_sockets.empty();
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

    auto socketPtr = socket.get();
    socket->cancelIOAsync(
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
