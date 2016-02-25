#include "multiple_server_socket.h"

#include <boost/optional.hpp>

#include "nx/utils/log/log.h"
#include "nx/utils/log/log_message.h"

namespace nx {
namespace network {

MultipleServerSocket::MultipleServerSocket()
    : m_nonBlockingMode(false)
    , m_recvTmeout(0)
    , m_terminated(nullptr)
    , m_timerSocket(new TCPSocket())
{
}

MultipleServerSocket::~MultipleServerSocket()
{
    if (m_terminated)
        *m_terminated = true;
}

// deliver option to every socket
#define MultipleServerSocket_FORWARD_SET(NAME, TYPE)    \
    bool MultipleServerSocket::NAME(TYPE value)         \
    {                                                   \
        for (auto& socket : m_serverSockets)            \
            if (!socket->NAME(value))                   \
            {                                           \
                socket->getLastError(&m_lastError);     \
                return false;                           \
            }                                           \
                                                        \
        return true;                                    \
    }                                                   \

// assume all sockets are configured the same way
#define MultipleServerSocket_FORWARD_GET(NAME, TYPE)    \
    bool MultipleServerSocket::NAME(TYPE* value) const  \
    {                                                   \
        boost::optional<TYPE> first;                    \
        for (auto& socket : m_serverSockets)            \
        {                                               \
            if (!socket->NAME(value))                   \
            {                                           \
                socket->getLastError(&m_lastError);     \
                return false;                           \
            }                                           \
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

MultipleServerSocket_FORWARD_SET(bind, const SocketAddress&)

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

void MultipleServerSocket::shutdown()
{
    for (auto& socket : m_serverSockets)
        socket->shutdown();
}

bool MultipleServerSocket::isClosed() const
{
    for (auto& socket : m_serverSockets)
        if (!socket->isClosed())
            return false;

    return true;
}

MultipleServerSocket_FORWARD_SET(setReuseAddrFlag, bool);
MultipleServerSocket_FORWARD_GET(getReuseAddrFlag, bool);

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

MultipleServerSocket_FORWARD_GET(getMtu, unsigned int);
MultipleServerSocket_FORWARD_SET(setSendBufferSize, unsigned int);
MultipleServerSocket_FORWARD_GET(getSendBufferSize, unsigned int);
MultipleServerSocket_FORWARD_SET(setRecvBufferSize, unsigned int);
MultipleServerSocket_FORWARD_GET(getRecvBufferSize, unsigned int);

bool MultipleServerSocket::setRecvTimeout(unsigned int millis)
{
    m_recvTmeout = millis;
    return true;
}

bool MultipleServerSocket::getRecvTimeout(unsigned int* millis) const
{
    *millis = m_recvTmeout;
    return true;
}

MultipleServerSocket_FORWARD_SET(setSendTimeout, unsigned int);
MultipleServerSocket_FORWARD_GET(getSendTimeout, unsigned int);

bool MultipleServerSocket::getLastError(SystemError::ErrorCode* errorCode) const
{
    *errorCode = m_lastError;
    m_lastError = SystemError::noError;
    return true;
}

AbstractSocket::SOCKET_HANDLE MultipleServerSocket::handle() const
{
    Q_ASSERT(false);
    return (AbstractSocket::SOCKET_HANDLE)(-1);
}

aio::AbstractAioThread* MultipleServerSocket::getAioThread()
{
    return m_timerSocket->getAioThread();
}

void MultipleServerSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_timerSocket->bindToAioThread(aioThread);
    for (auto& socket : m_serverSockets)
        socket->bindToAioThread(aioThread);
}

MultipleServerSocket_FORWARD_SET(listen, int);

AbstractStreamSocket* MultipleServerSocket::accept()
{
    if (m_nonBlockingMode)
    {
        for (auto& server : m_serverSockets)
            if (auto socket = server->accept())
                return socket;

        return nullptr;
    }

    std::promise<std::pair<SystemError::ErrorCode, AbstractStreamSocket*>> promise;
    acceptAsync([&](SystemError::ErrorCode code, AbstractStreamSocket* socket)
    {
        promise.set_value(std::make_pair(code, socket));
    });

    const auto result = promise.get_future().get();
    if (result.first != SystemError::noError)
    {
        m_lastError = result.first;
        SystemError::setLastErrorCode(result.first);
    }

    return result.second;
}

void MultipleServerSocket::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    nx::BarrierHandler barrier(std::move(handler));
    m_timerSocket->pleaseStop(barrier.fork());
    for (auto& socket : m_serverSockets)
        socket->pleaseStop(barrier.fork());
}

void MultipleServerSocket::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_timerSocket->post(std::move(handler));
}

void MultipleServerSocket::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_timerSocket->dispatch(std::move(handler));
}

void MultipleServerSocket::acceptAsync(
    std::function<void(SystemError::ErrorCode,
                       AbstractStreamSocket*)> handler)
{
    dispatch([this, handler]()
    {
        Q_ASSERT_X(!m_acceptHandler, Q_FUNC_INFO, "concurrent accept call");
        m_acceptHandler = std::move(handler);

        if (m_recvTmeout)
            m_timerSocket->registerTimer(m_recvTmeout, std::bind(
                &MultipleServerSocket::accepted, this,
                nullptr, SystemError::timedOut, nullptr));

        for (auto& socket : m_serverSockets)
            if (!socket.isAccepting)
            {
                socket.isAccepting = true;
                NX_LOGX(lm("accept on %1").arg(&socket), cl_logDEBUG2);

                using namespace std::placeholders;
                socket->acceptAsync(std::bind(
                    &MultipleServerSocket::accepted, this, &socket, _1, _2));
            }
    });
}

MultipleServerSocket::ServerSocketHandle::ServerSocketHandle(
        std::unique_ptr<AbstractStreamServerSocket> socket_)
    : socket(std::move(socket_))
    , isAccepting(false)
{
}

MultipleServerSocket::ServerSocketHandle::ServerSocketHandle(
        MultipleServerSocket::ServerSocketHandle&& right)
    : socket(std::move(right.socket))
    , isAccepting(std::move(right.isAccepting))
{
}

AbstractStreamServerSocket*
    MultipleServerSocket::ServerSocketHandle::operator->() const
{
    return socket.get();
}

void MultipleServerSocket::ServerSocketHandle::stopAccepting()
{
    if (!isAccepting)
        return;

    isAccepting = false;
    socket->pleaseStopSync();
}

bool MultipleServerSocket::addSocket(
        std::unique_ptr<AbstractStreamServerSocket> socket)
{
    // all internal sockets are used in non blocking mode while
    // interface allows both
    if (!socket->setNonBlockingMode(true))
        return false;

    socket->bindToAioThread(m_timerSocket->getAioThread());
    m_serverSockets.push_back(ServerSocketHandle(std::move(socket)));
    return true;
}

void MultipleServerSocket::accepted(
        ServerSocketHandle* source, SystemError::ErrorCode code,
        AbstractStreamSocket* rawSocket)
{
    std::unique_ptr<AbstractStreamSocket> socket(rawSocket);
    if (socket)
        socket->setNonBlockingMode(m_nonBlockingMode);

    NX_LOGX(lm("accepted %1 (%2) from %3")
            .arg(rawSocket).arg(SystemError::toString(code)).arg(source),
            cl_logDEBUG2);

    if (source)
    {
        source->isAccepting = false;
        m_timerSocket->pleaseStopSync(); // will not block, we are in IO thread
    }

    std::function<void(SystemError::ErrorCode, AbstractStreamSocket*)> handler;
    std::swap(handler, m_acceptHandler);
    Q_ASSERT_X(handler, Q_FUNC_INFO, "acceptAsync was not canceled in time");

    // TODO: #mux This is not efficient to cancel accepts on every accepted event
    //            (in most cases handler will call accept again).
    //            Hovewer this socket she'll be deletable right after a handler
    //            is handled, so we can not take any decisions afterhand.
    //            Consider using condition variables!

    NX_LOGX(lm("cancel all other accepts"), cl_logDEBUG2);
    for (auto& socket : m_serverSockets)
        socket.stopAccepting();

    handler(code, socket.release());
}

} // namespace network
} // namespace nx
