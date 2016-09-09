#include "reverse_connection_holder.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

ReverseConnectionHolder::ReverseConnectionHolder(aio::AbstractAioThread* aioThread):
    aio::BasicPollable(aioThread),
    m_socketCount(0)
{
}

void ReverseConnectionHolder::stopWhileInAioThread()
{
}

void ReverseConnectionHolder::saveSocket(std::unique_ptr<AbstractStreamSocket> socket)
{
    if (!m_handlers.empty())
    {
        auto handler = std::move(m_handlers.begin()->second);
        m_handlers.erase(m_handlers.begin());
        return handler(SystemError::noError, std::move(socket));
    }

    const auto it = m_sockets.insert(m_sockets.end(), std::move(socket));
    ++m_socketCount;
    (*it)->bindToAioThread(getAioThread());
    monitorSocket(it);
}

size_t ReverseConnectionHolder::socketCount() const
{
    return m_socketCount;
}

void ReverseConnectionHolder::takeSocket(std::chrono::milliseconds timeout, Handler handler)
{
    post(
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
                    timer()->start(
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
