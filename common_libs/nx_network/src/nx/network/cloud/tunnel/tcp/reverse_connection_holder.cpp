#include "reverse_connection_holder.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

ReverseConnectionHolder::ReverseConnectionHolder(aio::AbstractAioThread* aioThread):
    aio::BasicPollable(aioThread),
    m_socketCount(0),
    m_timer(std::make_unique<aio::Timer>())
{
    m_timer->bindToAioThread(aioThread);
}

ReverseConnectionHolder::~ReverseConnectionHolder()
{
    stopWhileInAioThread();
}

void ReverseConnectionHolder::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    aio::BasicPollable::bindToAioThread(aioThread);
    m_timer->bindToAioThread(aioThread);

    NX_ASSERT(m_socketCount, "Thread can not be changed in working state");
    for (auto& socket: m_sockets)
        socket->bindToAioThread(aioThread);
}

void ReverseConnectionHolder::stopWhileInAioThread()
{
    m_timer.reset();
    m_socketCount = 0;
    m_sockets.clear();
}

void ReverseConnectionHolder::saveSocket(std::unique_ptr<AbstractStreamSocket> socket)
{
    NX_ASSERT(isInSelfAioThread());
    if (!m_handlers.empty())
    {
        NX_LOGX(lm("Use new socket(%1), %2 sockets left")
            .args(socket, m_socketCount.load()), cl_logDEBUG2);

        auto handler = std::move(m_handlers.begin()->second);
        m_handlers.erase(m_handlers.begin());
        return handler(SystemError::noError, std::move(socket));
    }

    const auto it = m_sockets.insert(m_sockets.end(), std::move(socket));
    ++m_socketCount;
    NX_LOGX(lm("One socket(%1) added, %2 sockets left")
        .args(*it, m_socketCount.load()), cl_logDEBUG2);

    (*it)->bindToAioThread(getAioThread());
    monitorSocket(it);
}

size_t ReverseConnectionHolder::socketCount() const
{
    return m_socketCount;
}

void ReverseConnectionHolder::takeSocket(std::chrono::milliseconds timeout, Handler handler)
{
    // TODO: #ak Generally, zero timeout for socket operations means "no timeout".
    // Here it isn't so. But I'm not sure that making it so will not cause problems.
    // Have to investigate.

    post(
        [this, expirationTime = std::chrono::steady_clock::now() + timeout,
            handler = std::move(handler)]() mutable
        {
            if (!m_sockets.empty())
            {
                auto socket = std::move(m_sockets.front());
                --m_socketCount;
                m_sockets.pop_front();
                NX_LOGX(lm("One socket(%1) used, %2 sockets left")
                    .args(socket, m_socketCount.load()), cl_logDEBUG2);

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
                    startCleanupTimer(timeLeft);

                m_handlers.emplace(expirationTime, std::move(handler));
            }
        });
}

void ReverseConnectionHolder::startCleanupTimer(std::chrono::milliseconds timeLeft)
{
    m_timer->start(
        timeLeft,
        [this]()
        {
            const auto now = std::chrono::steady_clock::now();
            for (auto it = m_handlers.begin(); it != m_handlers.end(); )
            {
                const auto timeLeft = std::chrono::duration_cast<std::chrono::milliseconds>(
                    it->first - now);

                if (timeLeft.count() > 0)
                    return startCleanupTimer(timeLeft);

                it->second(SystemError::timedOut, nullptr);
                it = m_handlers.erase(it);
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

            if (code != SystemError::noError || size == 0)
            {
                NX_LOGX(lm("Connection(%1) has been closed: %2 (%3 sockets left)")
                    .args(*it, SystemError::toString(code), m_socketCount.load() - 1),
                    cl_logDEBUG1);
            }
            else
            {
                NX_LOGX(lm("Unexpected read on socket(%1), size=%2. Closing socket (%3 sockets left)")
                    .args(*it, size, m_socketCount.load() - 1),
                    cl_logERROR);
            }

            (void)buffer; //< This buffer might be helpful for debug is case smth goes wrong!
            --m_socketCount;
            m_sockets.erase(it);
        });
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
