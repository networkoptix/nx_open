#include "multiple_server_socket.h"

#include "boost/optional.hpp"

namespace nx {
namespace network {

MultipleServerSocket::MultipleServerSocket()
    : m_nonBlockingMode(false)
{
}

MultipleServerSocket::~MultipleServerSocket()
{
    if (!m_nonBlockingMode)
        for (auto& socket : m_serverSockets)
            socket->pleaseStopSync();
}

// deliver option to every socket
#define CloudServerSocket_FORWARD_SET(NAME, TYPE)   \
    bool MultipleServerSocket::NAME(TYPE value)     \
    {                                               \
        for (auto& socket : m_serverSockets)        \
            if (!socket->NAME(value))               \
                return false;                       \
                                                    \
        return true;                                \
    }                                               \

// assume all sockets are configured the same way
#define CloudServerSocket_FORWARD_GET(NAME, TYPE)       \
    bool MultipleServerSocket::NAME(TYPE* value) const  \
    {                                                   \
        boost::optional<TYPE> first;                    \
        for (auto& socket : m_serverSockets)            \
        {                                               \
            if (!socket->NAME(value))                   \
                return false;                           \
                                                        \
            if (first)                                  \
                Q_ASSERT_X(*first == *value,            \
                    Q_FUNC_INFO, QString("%1 != %2")    \
                        .arg(*first).arg(*value)        \
                        .toStdString().c_str());        \
            else                                        \
                first = *value;                         \
        }                                               \
                                                        \
        return true;                                    \
    }

CloudServerSocket_FORWARD_SET(bind, const SocketAddress&)

SocketAddress MultipleServerSocket::getLocalAddress() const
{
    boost::optional<SocketAddress> first;
    for (auto& socket : m_serverSockets)
    {
        const auto value = socket->getLocalAddress();
        if (first)
            Q_ASSERT(*first == value);
        else
            first = value;
    }

    return *first;
}

void MultipleServerSocket::close()
{
    for (auto& socket : m_serverSockets)
        socket->close();
}

bool MultipleServerSocket::isClosed() const
{
    for (auto& socket : m_serverSockets)
        if (!socket->isClosed())
            return false;

    return true;
}

CloudServerSocket_FORWARD_SET(setReuseAddrFlag, bool);
CloudServerSocket_FORWARD_GET(getReuseAddrFlag, bool);

bool MultipleServerSocket::setNonBlockingMode(bool value)
{
    m_nonBlockingMode = value;
    return true;
}

bool MultipleServerSocket::getNonBlockingMode(bool* value) const
{
    *value = m_nonBlockingMode;
    return true;
}

CloudServerSocket_FORWARD_GET(getMtu, unsigned int);
CloudServerSocket_FORWARD_SET(setSendBufferSize, unsigned int);
CloudServerSocket_FORWARD_GET(getSendBufferSize, unsigned int);
CloudServerSocket_FORWARD_SET(setRecvBufferSize, unsigned int);
CloudServerSocket_FORWARD_GET(getRecvBufferSize, unsigned int);
CloudServerSocket_FORWARD_SET(setRecvTimeout, unsigned int);
CloudServerSocket_FORWARD_GET(getRecvTimeout, unsigned int);
CloudServerSocket_FORWARD_SET(setSendTimeout, unsigned int);
CloudServerSocket_FORWARD_GET(getSendTimeout, unsigned int);

bool MultipleServerSocket::getLastError(SystemError::ErrorCode* errorCode) const
{
    // TODO: #mux does it make any sense?
    *errorCode = SystemError::getLastOSErrorCode();
    return true;
}

AbstractSocket::SOCKET_HANDLE MultipleServerSocket::handle() const
{
    Q_ASSERT(false);
    return (AbstractSocket::SOCKET_HANDLE)(-1);
}

CloudServerSocket_FORWARD_SET(listen, int);

AbstractStreamSocket* MultipleServerSocket::accept()
{
    std::promise<std::pair<SystemError::ErrorCode, AbstractStreamSocket*>> promise;
    acceptAsyncImpl([&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
    {
        promise.set_value(std::make_pair(code, socket));
    });

    const auto result = promise.get_future().get();
    if (result.first != SystemError::noError)
        SystemError::setLastErrorCode(result.first);

    return result.second;
}

void MultipleServerSocket::pleaseStop(std::function<void()> handler)
{
    nx::BarrierHandler barrier(std::move(handler));
    for (auto& socket : m_serverSockets)
        socket->pleaseStop(barrier.fork());
}

void MultipleServerSocket::postImpl( std::function<void()>&& handler )
{
    m_serverSockets.front()->post(std::move(handler));
}

void MultipleServerSocket::dispatchImpl( std::function<void()>&& handler )
{
    m_serverSockets.front()->dispatch(std::move(handler));
}

void MultipleServerSocket::acceptAsyncImpl(
    std::function<void(SystemError::ErrorCode,
                       AbstractStreamSocket*)>&& handler)
{
    unsigned int recvTimeout;
    if (!getRecvTimeout(&recvTimeout))
        return post(std::bind(
            std::move(handler), SystemError::invalidData, nullptr));

    const auto limit = std::chrono::system_clock::now() -
                       std::chrono::milliseconds(recvTimeout);

    QnMutexLocker lk(&m_mutex);
    Q_ASSERT(!m_acceptHandler);
    while (!m_acceptedInfos.empty())
    {
        auto info = std::move(m_acceptedInfos.front());
        m_acceptedInfos.pop();

        // return accepted socket if it isn't expired
        if (!recvTimeout || info.time < limit)
        {
            lk.unlock();
            return handler(info.code, info.socket.release());
        }
    }

    m_acceptHandler = std::move(handler);
    for (auto& socket : m_serverSockets)
    {
        if (!socket.isAccepting)
        {
            socket.isAccepting = true;
            using namespace std::placeholders;
            socket->acceptAsync(std::bind(
                &MultipleServerSocket::accepted, this, &socket, _1, _2));
        }
    }
}

MultipleServerSocket::ServerSocketData::ServerSocketData(
        std::unique_ptr<AbstractStreamServerSocket> socket_)
    : socket(std::move(socket_))
    , isAccepting(false)
{
}

AbstractStreamServerSocket*
    MultipleServerSocket::ServerSocketData::ServerSocketData::operator->() const
{
    return socket.get();
}

MultipleServerSocket::AcceptedData::AcceptedData(
        SystemError::ErrorCode code_, AbstractStreamSocket* socket_)
    : code(code_)
    , socket(socket_)
    , time(std::chrono::system_clock::now())
{
}

bool MultipleServerSocket::addSocket(
        std::unique_ptr<AbstractStreamServerSocket> socket)
{
    // all internal sockets are used in non blocking mode while
    // interface allows both
    if (!socket->setNonBlockingMode(true))
        return false;

    m_serverSockets.push_back(ServerSocketData(std::move(socket)));
    return true;
}

void MultipleServerSocket::accepted(
        ServerSocketData* source, SystemError::ErrorCode code,
        AbstractStreamSocket* socket)
{
    if (!m_nonBlockingMode)
        socket->setNonBlockingMode(false);

    QnMutexLocker lk(&m_mutex);
    source->isAccepting = false;
    if (m_acceptHandler)
    {
        const auto handler = std::move(m_acceptHandler);
        lk.unlock();
        return handler(code, socket);
    }

    m_acceptedInfos.push(AcceptedData(code, socket));
}

} // namespace cloud
} // namespace network
