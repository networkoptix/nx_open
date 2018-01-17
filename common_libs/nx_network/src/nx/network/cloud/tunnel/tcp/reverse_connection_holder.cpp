#include "reverse_connection_holder.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

static std::chrono::milliseconds s_connectionLessHolderExpirationTimeout = std::chrono::seconds(31);

void ReverseConnectionHolder::setConnectionlessHolderExpirationTimeout(
    std::chrono::milliseconds value)
{
    s_connectionLessHolderExpirationTimeout = value;
}

ReverseConnectionHolder::ReverseConnectionHolder(
    aio::AbstractAioThread* aioThread,
    const String& hostName)
    :
    aio::BasicPollable(aioThread),
    m_hostName(hostName),
    m_socketCount(0),
    m_timer(std::make_unique<aio::Timer>()),
    m_prevConnectionTime(std::chrono::steady_clock::now())
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
        NX_LOGX(lm("Host %1. Using newly-acquired socket(%2), %3 sockets left")
            .args(m_hostName, socket, m_socketCount.load()), cl_logDEBUG1);

        auto handler = std::move(m_handlers.begin()->second);
        m_handlers.erase(m_handlers.begin());
        return handler(SystemError::noError, std::move(socket));
    }

    const auto it = m_sockets.insert(m_sockets.end(), std::move(socket));
    ++m_socketCount;
    NX_LOGX(lm("Host %1. One socket(%2) added, %3 sockets left")
        .args(m_hostName, *it, m_socketCount.load()), cl_logDEBUG1);

    (*it)->bindToAioThread(getAioThread());
    monitorSocket(it);

    m_prevConnectionTime = std::chrono::steady_clock::now();
}

bool ReverseConnectionHolder::isActive() const
{
    const auto expirationTime = m_prevConnectionTime + s_connectionLessHolderExpirationTimeout;
    return
        m_socketCount > 0 ||
        (std::chrono::steady_clock::now() < expirationTime);
}

size_t ReverseConnectionHolder::socketCount() const
{
    return m_socketCount;
}

// Code here is not written to work without timeout.
// So, instead of "no timeout" using timeout that is unusually large for sockets.
static const std::chrono::milliseconds kInfiniteTimeoutEmulation(std::chrono::hours(2));

void ReverseConnectionHolder::takeSocket(std::chrono::milliseconds timeout, Handler handler)
{
    if (timeout == nx::network::kNoTimeout)
        timeout = kInfiniteTimeoutEmulation;

    post(
        [this, expirationTime = std::chrono::steady_clock::now() + timeout,
            handler = std::move(handler)]() mutable
        {
            if (!m_sockets.empty())
            {
                auto socket = std::move(m_sockets.front());
                --m_socketCount;
                m_sockets.pop_front();
                NX_LOGX(lm("Host %1. One socket(%2) used, %3 sockets left")
                    .args(m_hostName, socket, m_socketCount.load()), cl_logDEBUG1);

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

                NX_LOGX(lm("Added pending receiver for connection to %1. Totally %2 receives pending")
                    .args(m_hostName, m_handlers.size()), cl_logDEBUG1);
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
                NX_LOGX(lm("Host %1. Connection(%2) has been closed: %3 (%4 sockets left)")
                    .args(m_hostName, *it, SystemError::toString(code), m_socketCount.load() - 1),
                    cl_logDEBUG1);
            }
            else
            {
                NX_LOGX(lm("Host %1. Unexpected read on socket(%2), size=%3. "
                    "Closing socket (%4 sockets left)")
                    .args(m_hostName, *it, size, m_socketCount.load() - 1), cl_logERROR);
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
