#include "multiple_server_socket.h"

#include <boost/optional.hpp>

#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>

#include "stream_server_socket_to_acceptor_wrapper.h"
#include "socket_global.h"

namespace nx {
namespace network {

MultipleServerSocket::MultipleServerSocket():
    m_nonBlockingMode(false),
    m_recvTmeout(0),
    m_terminated(nullptr)
{
    bindToAioThread(getAioThread());
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
        TYPE firstValue{};                              \
        bool firstValueFilled = false;                  \
        for (auto& socket: m_serverSockets)             \
        {                                               \
            if (!socket->NAME(value))                   \
            {                                           \
                socket->getLastError(&m_lastError);     \
                return false;                           \
            }                                           \
                                                        \
            if (!firstValueFilled)                      \
            {                                           \
                firstValue = *value;                    \
                firstValueFilled = true;                \
            }                                           \
                                                        \
            NX_ASSERT(firstValue == *value,             \
                Q_FUNC_INFO, QString("%1 != %2")        \
                    .arg(firstValue).arg(*value)        \
                    .toStdString().c_str());            \
        }                                               \
                                                        \
        return true;                                    \
    }

MultipleServerSocket_FORWARD_SET(bind, const SocketAddress&)

SocketAddress MultipleServerSocket::getLocalAddress() const
{
    for (auto& socket: m_serverSockets)
    {
        const auto endpoint = socket->getLocalAddress();
        if (endpoint.port > 0)
            return endpoint;
    }

    return SocketAddress();
}

bool MultipleServerSocket::close()
{
    bool result = true;
    for (auto& socket: m_serverSockets)
        result &= socket->close();
    return result;
}

bool MultipleServerSocket::shutdown()
{
    bool result = true;
    for (auto& socket: m_serverSockets)
        result &= socket->shutdown();
    return result;
}

bool MultipleServerSocket::isClosed() const
{
    for (auto& socket: m_serverSockets)
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
    if (millis == 0)
        m_aggregateAcceptor.setAcceptTimeout(boost::none);
    else
        m_aggregateAcceptor.setAcceptTimeout(std::chrono::milliseconds(millis));
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
    m_aggregateAcceptor.bindToAioThread(aioThread);
}

bool MultipleServerSocket::isInSelfAioThread() const
{
    return m_timer.isInSelfAioThread();
}

MultipleServerSocket_FORWARD_SET(listen, int);

AbstractStreamSocket* MultipleServerSocket::accept()
{
    NX_VERBOSE(this, lm("accept()"));
    if (m_nonBlockingMode)
    {
        for (auto& server : m_serverSockets)
            if (auto socket = server->accept())
                return socket;

        return nullptr;
    }

    nx::utils::promise<
        std::pair<SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>>
    > promise;
    acceptAsync(
        [this, &promise](
            SystemError::ErrorCode code,
            std::unique_ptr<AbstractStreamSocket> rawSocket)
        {
            // Here we have to post, to make all of m_serverSockets good to remove
            // right after sync accept is returned.
            post(
                [this, &promise, code, socket = std::move(rawSocket)]() mutable
                {
                    NX_VERBOSE(this, lm("accept() returns %1").arg(socket));
                    promise.set_value(std::make_pair(code, std::move(socket)));
                });
        });

    auto result = promise.get_future().get();
    if (result.first != SystemError::noError)
    {
        m_lastError = result.first;
        SystemError::setLastErrorCode(result.first);
    }
    else
    {
        result.second->setNonBlockingMode(false);
    }

    return result.second.release();
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

void MultipleServerSocket::acceptAsync(AcceptCompletionHandler handler)
{
    m_aggregateAcceptor.acceptAsync(std::move(handler));
}

void MultipleServerSocket::cancelIOAsync(nx::utils::MoveOnlyFunc<void()> handler)
{
    NX_VERBOSE(this, lm("Canceling async IO asynchronously..."));
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
    NX_VERBOSE(this, lm("Canceling async IO synchronously..."));
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

bool MultipleServerSocket::addSocket(
    std::unique_ptr<AbstractStreamServerSocket> socket)
{
    NX_VERBOSE(this, lm("Add socket(%1)").arg(socket));
    if (!socket->setNonBlockingMode(true))
        return false;

    auto socketPtr = socket.get();
    if (m_aggregateAcceptor.add(
            std::make_unique<StreamServerSocketToAcceptorWrapper>(std::move(socket))))
    {
        m_serverSockets.push_back(socketPtr);
        return true;
    }
    return false;
}

void MultipleServerSocket::removeSocket(size_t pos)
{
    m_aggregateAcceptor.removeAt(pos);
    m_serverSockets.erase(m_serverSockets.begin() + pos);
}

size_t MultipleServerSocket::count() const
{
    return m_serverSockets.size();
}

void MultipleServerSocket::cancelIoFromAioThread()
{
    m_acceptHandler = nullptr;
    m_timer.cancelSync();
    m_aggregateAcceptor.cancelIOSync();
}

void MultipleServerSocket::stopWhileInAioThread()
{
    NX_LOGX(lm("Stopped"), cl_logDEBUG1);
    m_serverSockets.clear();
    m_aggregateAcceptor.pleaseStopSync();

    NX_ASSERT(m_timer.isInSelfAioThread());
    m_timer.pleaseStopSync(false);
}

} // namespace network
} // namespace nx
