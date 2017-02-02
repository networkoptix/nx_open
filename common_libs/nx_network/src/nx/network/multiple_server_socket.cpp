#include "multiple_server_socket.h"

#include <boost/optional.hpp>

#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>

#include "socket_global.h"

#define DEBUG_LOG(MESSAGE) do \
{ \
    if (nx::network::SocketGlobals::debugConfig().multipleServerSocket) \
        NX_LOGX(MESSAGE, cl_logDEBUG1); \
} while (0)

namespace nx {
namespace network {

MultipleServerSocket::MultipleServerSocket()
    : m_nonBlockingMode(false)
    , m_recvTmeout(0)
    , m_terminated(nullptr)
{
}

MultipleServerSocket::~MultipleServerSocket()
{
    if (m_terminated)
        *m_terminated = true;

    if (isInSelfAioThread())
        stopWhileInAioThread();
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
                NX_ASSERT(*first == *value,            \
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
            NX_ASSERT(*first == value);
        else
            first = value;
    }

    return *first;
}

bool MultipleServerSocket::close()
{
    bool result = true;
    for (auto& socket : m_serverSockets)
        result &= socket->close();
    return result;
}

bool MultipleServerSocket::shutdown()
{
    bool result = true;
    for (auto& socket : m_serverSockets)
        result &= socket->shutdown();
    return result;
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
    NX_ASSERT(false);
    return (AbstractSocket::SOCKET_HANDLE)(-1);
}

aio::AbstractAioThread* MultipleServerSocket::getAioThread() const
{
    return m_timer.getAioThread();
}

void MultipleServerSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_timer.bindToAioThread(aioThread);
    for (auto& socket : m_serverSockets)
        socket->bindToAioThread(aioThread);
}

bool MultipleServerSocket::isInSelfAioThread() const
{
    return m_timer.isInSelfAioThread();
}

MultipleServerSocket_FORWARD_SET(listen, int);

AbstractStreamSocket* MultipleServerSocket::accept()
{
    DEBUG_LOG(lm("accept()"));
    if (m_nonBlockingMode)
    {
        for (auto& server : m_serverSockets)
            if (auto socket = server->accept())
                return socket;

        return nullptr;
    }

    nx::utils::promise<std::pair<SystemError::ErrorCode, AbstractStreamSocket*>> promise;
    acceptAsync(
        [this, &promise](SystemError::ErrorCode code, AbstractStreamSocket* rawSocket)
        {
            std::unique_ptr<AbstractStreamSocket> socket(rawSocket);

            // Here we have to post, to make all of m_serverSockets good to remove
            // right after sync accept is returned.
            post(
                [this, &promise, code, socket = std::move(socket)]() mutable
                {
                    DEBUG_LOG(lm("accept() returns %1").arg(socket));
                    promise.set_value(std::make_pair(code, socket.release()));
                });
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
    NX_LOGX(lm("Stopping..."), cl_logDEBUG2);
    m_timer.pleaseStop(
        [this, handler = std::move(handler)]()
        {
            stopWhileInAioThread();
            handler();
        });
}

void MultipleServerSocket::pleaseStopSync(bool assertIfCalledUnderLock)
{
    if (m_timer.isInSelfAioThread())
        stopWhileInAioThread();
    else
        QnStoppableAsync::pleaseStopSync(assertIfCalledUnderLock);
}

Pollable* MultipleServerSocket::pollable()
{
    NX_CRITICAL(false, "Not implemented");
    return nullptr;
}

void MultipleServerSocket::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_timer.post(std::move(handler));
}

void MultipleServerSocket::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_timer.dispatch(std::move(handler));
}

void MultipleServerSocket::acceptAsync(
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        AbstractStreamSocket*)> handler)
{
    dispatch([this, handler = std::move(handler)]() mutable
    {
        NX_ASSERT(!m_acceptHandler, Q_FUNC_INFO, "concurrent accept call");
        m_acceptHandler = std::move(handler);

        if (m_recvTmeout)
        {
            m_timer.start(
                std::chrono::milliseconds(m_recvTmeout),
                std::bind(
                    &MultipleServerSocket::accepted, this,
                    nullptr, SystemError::timedOut, nullptr));
        }

        for (auto& source : m_serverSockets)
        {
            if (!source.isAccepting)
            {
                source.isAccepting = true;
                DEBUG_LOG(lm("Accept on source(%1)").arg(&source));

                using namespace std::placeholders;
                source->acceptAsync(std::bind(
                    &MultipleServerSocket::accepted, this, &source, _1, _2));
            }
        }
    });
}

void MultipleServerSocket::cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
{
    DEBUG_LOG(lm("Canceling async IO asynchronously..."));
    post(
        [this, handler = std::move(handler)]() mutable
        {
            cancelIoFromAioThread();
            NX_LOGX(lm("Async IO is canceled asynchronously"), cl_logDEBUG1);
            handler();
        });
}

void MultipleServerSocket::cancelIOSync()
{
    DEBUG_LOG(lm("Canceling async IO synchronously..."));
    nx::utils::promise<void> ioCancelledPromise;
    dispatch(
        [this, &ioCancelledPromise]() mutable
        {
            cancelIoFromAioThread();
            NX_LOGX(lm("Async IO is canceled synchronously"), cl_logDEBUG1);
            ioCancelledPromise.set_value();
        });

    ioCancelledPromise.get_future().wait();
}

MultipleServerSocket::ServerSocketContext::ServerSocketContext(
    std::unique_ptr<AbstractStreamServerSocket> socket_)
    :
    socket(std::move(socket_)),
    isAccepting(false)
{
}

AbstractStreamServerSocket*
    MultipleServerSocket::ServerSocketContext::operator->() const
{
    return socket.get();
}

void MultipleServerSocket::ServerSocketContext::stopAccepting()
{
    if (!isAccepting)
        return;

    isAccepting = false;
    socket->cancelIOSync();
}

bool MultipleServerSocket::addSocket(
    std::unique_ptr<AbstractStreamServerSocket> socket)
{
    DEBUG_LOG(lm("Add socket(%1)").arg(socket));
    if (!socket->setNonBlockingMode(true))
        return false;

    //NOTE socket count MUST be updated without delay

    nx::utils::promise<void> socketAddedPromise;
    dispatch(
        [this, &socketAddedPromise, socket = std::move(socket)]() mutable
        {
            // all internal sockets are used in non blocking mode while
            // interface allows both

            socket->bindToAioThread(m_timer.getAioThread());
            m_serverSockets.push_back(ServerSocketContext(std::move(socket)));
            if (m_acceptHandler)
            {
                ServerSocketContext& source = m_serverSockets.back();
                source.isAccepting = true;
                NX_LOGX(lm("Accept on source(%1) when added").arg(&source), cl_logDEBUG1);

                using namespace std::placeholders;
                source->acceptAsync(std::bind(
                    &MultipleServerSocket::accepted, this, &source, _1, _2));
            }

            socketAddedPromise.set_value();
        });

    socketAddedPromise.get_future().wait();
    return true;
}

void MultipleServerSocket::removeSocket(size_t pos)
{
    DEBUG_LOG(lm("Remove socket(%1)").arg(pos));
    nx::utils::promise<void> socketRemovedPromise;
    dispatch(
        [this, &socketRemovedPromise, pos]()
        {
            if (pos >= m_serverSockets.size())
            {
                NX_ASSERT(false, lm("pos = %1, m_serverSockets.size() = %2")
                    .arg(pos).arg(m_serverSockets.size()));
                return;
            }

            auto itemToRemoveIter = std::next(m_serverSockets.begin(), pos);
            auto serverSocketContext = std::move(*itemToRemoveIter);
            m_serverSockets.erase(itemToRemoveIter);
            serverSocketContext.socket->pleaseStopSync();

            NX_LOGX(lm("Socket(%1) is removed").arg(serverSocketContext.socket), cl_logDEBUG1);
            socketRemovedPromise.set_value();
        });

    socketRemovedPromise.get_future().wait();
}

size_t MultipleServerSocket::count() const
{
    return m_serverSockets.size();
}

void MultipleServerSocket::accepted(
    ServerSocketContext* source,
    SystemError::ErrorCode code,
    AbstractStreamSocket* rawSocket)
{
    std::unique_ptr<AbstractStreamSocket> socket(rawSocket);
    DEBUG_LOG(lm("Accepted socket(%1) (%2) from source(%3)")
        .arg(rawSocket).arg(SystemError::toString(code)).arg(source));

    if (source)
    {
        NX_CRITICAL(
            std::find_if(
                m_serverSockets.begin(), m_serverSockets.end(),
                [source](ServerSocketContext& item) { return source == &item; }) != 
            m_serverSockets.end());

        source->isAccepting = false;
        m_timer.cancelSync(); // will not block, we are in IO thread
    }

    auto handler = std::move(m_acceptHandler);
    m_acceptHandler = nullptr;
    NX_ASSERT(handler, Q_FUNC_INFO, "acceptAsync was not canceled in time");

    // TODO: #mux This is not efficient to cancel accepts on every accepted event
    //            (in most cases handler will call accept again).
    //            Hovewer this socket she'll be deletable right after a handler
    //            is handled, so we can not take any decisions afterhand.
    //            Consider using condition variables!

    DEBUG_LOG(lm("Cancel all other accepts"));
    for (auto& socketContext : m_serverSockets)
        socketContext.stopAccepting();

    handler(code, socket.release());
}

void MultipleServerSocket::cancelIoFromAioThread()
{
    m_acceptHandler = nullptr;
    m_timer.cancelSync();
    for (auto& socketContext: m_serverSockets)
        socketContext.stopAccepting();
}

void MultipleServerSocket::stopWhileInAioThread()
{
    NX_LOGX(lm("Stopped"), cl_logDEBUG1);
    m_serverSockets.clear();

    NX_ASSERT(m_timer.isInSelfAioThread());
    m_timer.pleaseStopSync(false);
}

} // namespace network
} // namespace nx
