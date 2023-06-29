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
    size_t operator()(const nx::network::ConnectionCache::ConnectionInfo& info) const
    {
        const size_t isTls = info.isTls ? 1 : 0;
        return std::hash<SocketAddress>{}(info.addr) ^ isTls;
    }
};

} // namespace std

std::string ConnectionCache::ConnectionInfo::toString() const
{
    return nx::utils::buildString("addr: ", addr.toString(), ", isTls: ", isTls ? "true" : "false");
}

//-------------------------------------------------------------------------------------------------

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
            nx::utils::TimeOutCache<ConnectionInfo, std::unique_ptr<AbstractStreamSocket>>& cache,
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
        nx::utils::TimeOutCache<ConnectionInfo, std::unique_ptr<AbstractStreamSocket>>& m_cache;
        std::atomic<size_t>& m_size;
    };

    nx::utils::TimeOutCache<ConnectionInfo, std::unique_ptr<AbstractStreamSocket>> cache;
    std::atomic<size_t> size;
    PollableContext pollableContext;
};

//-------------------------------------------------------------------------------------------------

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
            socket->bindToAioThread(d->pollableContext.getAioThread());
            startMonitoringConnection(addr, socket.get());
            d->cache.put(std::move(addr), std::move(socket));
            d->size = d->cache.size();

            NX_VERBOSE(this, "Store connection %1. Cache size %2", addr, d->size);
        });
}

void ConnectionCache::take(
    const ConnectionInfo& info,
    nx::utils::MoveOnlyFunc<void(std::unique_ptr<AbstractStreamSocket>)> handler)
{
    d->pollableContext.dispatch(
        [this, info, handler = std::move(handler)]() mutable
        {
            std::unique_ptr<AbstractStreamSocket> conn;
            auto result = d->cache.getValue(info);
            if (result.has_value())
            {
                NX_VERBOSE(this, "Connection %1 found", info);

                conn = std::move(result.value().get());
                d->cache.erase(info);
                conn->cancelRead();
            }
            else
            {
                NX_VERBOSE(this, "Connection %1 was not found", info);
            }

            d->size = d->cache.size();

            handler(std::move(conn));
        });
}

size_t ConnectionCache::size() const
{
    return d->size;
}

void ConnectionCache::startMonitoringConnection(
    const ConnectionInfo& addr,
    AbstractStreamSocket* conn)
{
    auto buf = std::make_unique<nx::Buffer>();
    buf->reserve(1);
    auto bufPtr = buf.get();

    conn->readSomeAsync(
        bufPtr,
        [this, buf = std::move(buf), addr](
            SystemError::ErrorCode result, std::size_t bytesRead)
        {
            // Unexpected read or connection closed. Remove connection from the cache.
            if (result != SystemError::noError)
                NX_DEBUG(this, "Connection %1 closed due to an error: %2", addr, result);
            else if (bytesRead > 0)
                NX_DEBUG(this, "Unexpected (%1 bytes) read through connection %2", bytesRead, addr);
            else
                NX_VERBOSE(this, "Connection %1 closed gracefully", addr);

            take(addr, [](auto&&...) {});
        });
}
