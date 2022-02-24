// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_cache.h"

#include <atomic>
#include <limits>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/data_structures/time_out_cache.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

using namespace nx::network;

namespace std {

template<>
class hash<nx::network::ConnectionCache::ConnectionInfo>
{
public:
    size_t operator()(const nx::network::ConnectionCache::ConnectionInfo& info) const;
};

} // namespace std

namespace {

class SocketKeeper: public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    SocketKeeper(
        ConnectionCache* cache,
        const nx::network::ConnectionCache::ConnectionInfo& addr,
        std::unique_ptr<AbstractStreamSocket> socket)
        :
        m_socket(std::move(socket)),
        m_buf(std::make_unique<nx::Buffer>())
    {
        m_socket->bindToAioThread(BasicPollable::getAioThread());
        m_buf->reserve(1);
        m_socket->readSomeAsync(
            m_buf.get(),
            [this, cache, addr](
                SystemError::ErrorCode result, std::size_t bytesRead)
            {
                // Unexpected read or connection closed. Remove connection from the cache.
                if (result != SystemError::noError)
                {
                    NX_DEBUG(this, "Connection closed due to an error: ", result);
                }
                else if (bytesRead > 0)
                {
                    NX_DEBUG(this, "Unexpected read from server.");
                }
                cache->take(addr, [](std::unique_ptr<AbstractStreamSocket>) {});
            });
    }

    SocketKeeper(SocketKeeper&& other) noexcept
    {
        std::swap(m_socket, other.m_socket);
        std::swap(m_buf, other.m_buf);
        NX_ASSERT(!m_socket || m_socket->isInSelfAioThread());
    }

    SocketKeeper& operator=(SocketKeeper&& rhs) noexcept
    {
        if (&rhs != this)
        {
            BasicPollable::pleaseStopSync();
            std::swap(m_socket, rhs.m_socket);
            std::swap(m_buf, rhs.m_buf);
            NX_ASSERT(!m_socket || m_socket->isInSelfAioThread());
        }
        return *this;
    }

    virtual ~SocketKeeper() noexcept override
    {
        NX_ASSERT(BasicPollable::isInSelfAioThread());
        BasicPollable::pleaseStopSync();
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        if (m_socket)
            m_socket->bindToAioThread(aioThread);
    }

    std::unique_ptr<AbstractStreamSocket> take() noexcept
    {
        NX_ASSERT(BasicPollable::isInSelfAioThread());
        if (m_socket)
            m_socket->cancelRead(); // I/O cancellation is non-blocking in socket's AIO thread.
        return std::exchange(m_socket, nullptr);
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        BasicPollable::stopWhileInAioThread();
        m_socket.reset();
    }

private:
    std::unique_ptr<AbstractStreamSocket> m_socket;
    std::unique_ptr<nx::Buffer> m_buf;
};

} // anonymous namespace

class ConnectionCache::Private
{
public:
    Private(
        std::chrono::milliseconds expirationPeriod,
        std::size_t maxSize)
        :
        cache(expirationPeriod, maxSize),
        size(0),
        pollableContext(cache, size)
    {
    }

    class PollableContext: public nx::network::aio::BasicPollable
    {
    public:
        PollableContext(
            nx::utils::TimeOutCache<ConnectionInfo, SocketKeeper>& cache,
            std::atomic<size_t>& size)
            :
            m_cache(cache),
            m_size(size)
        {
        }

    protected:
        virtual void stopWhileInAioThread() override
        {
            BasicPollable::stopWhileInAioThread();
            m_cache.clear();
            m_size = 0;
        }

    private:
        nx::utils::TimeOutCache<ConnectionInfo, SocketKeeper>& m_cache;
        std::atomic<size_t>& m_size;
    };

    nx::utils::TimeOutCache<ConnectionInfo, SocketKeeper> cache;
    std::atomic<size_t> size;
    PollableContext pollableContext;
};

size_t std::hash<nx::network::ConnectionCache::ConnectionInfo>::operator()(
    const nx::network::ConnectionCache::ConnectionInfo& info) const
{
    const size_t isTls = info.isTls ? 1 : 0;
    return std::hash<SocketAddress>{}(info.addr) ^ isTls;
}

ConnectionCache::ConnectionCache(
    std::chrono::milliseconds expirationPeriod /*= kDefaultExpirationPeriod*/)
    :
    d(std::make_unique<Private>(expirationPeriod, std::numeric_limits<size_t>::max()))
{
}

ConnectionCache::~ConnectionCache() noexcept
{
    if (d)
    {
        d->pollableContext.pleaseStopSync();
    }
}

ConnectionCache::ConnectionCache(ConnectionCache&&) noexcept = default;

ConnectionCache& ConnectionCache::operator=(ConnectionCache&& rhs) noexcept
{
    if (this != &rhs)
    {
        if (d)
            d->pollableContext.pleaseStopSync();
        d = std::move(rhs.d);
    }
    return *this;
}

void ConnectionCache::put(ConnectionInfo addr, std::unique_ptr<AbstractStreamSocket> socket)
{
    d->pollableContext.dispatch(
        [this, addr, socket = std::move(socket)]() mutable
        {
            SocketKeeper keeper(this, addr, std::move(socket));
            d->cache.put(std::move(addr), std::move(keeper));
            ++d->size;
        });
}

void ConnectionCache::take(
    const ConnectionInfo& info,
    nx::utils::MoveOnlyFunc<void(std::unique_ptr<AbstractStreamSocket>)> handler)
{
    d->pollableContext.dispatch(
        [this, info, handler = std::move(handler)]() mutable
        {
            auto result = d->cache.getValue(info);
            if (result.has_value())
            {
                auto value = std::move(result.value().get());
                d->cache.erase(info);
                --d->size;
                handler(value.take());
            }
            else
            {
                handler(nullptr);
            }
        });
}

size_t ConnectionCache::size() const
{
    return d->size;
}
