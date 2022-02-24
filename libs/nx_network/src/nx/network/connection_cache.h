// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <stddef.h>

#include <nx/network/socket_common.h>
#include <nx/network/abstract_socket.h>

namespace nx::network {

/**
 * Keeps opened connections associated with addresses.
 */
class NX_NETWORK_API ConnectionCache
{
public:
    /**
     * Connections will be deleted from the cache after this period of inactivity.
     */
    static constexpr std::chrono::milliseconds kDefaultExpirationPeriod = std::chrono::minutes(1);

    explicit ConnectionCache(
        std::chrono::milliseconds expirationPeriod = kDefaultExpirationPeriod);

    ~ConnectionCache() noexcept;

    ConnectionCache(ConnectionCache&&) noexcept;
    ConnectionCache& operator=(ConnectionCache&&) noexcept;

    struct ConnectionInfo
    {
        bool operator==(const ConnectionInfo&) const = default;

        SocketAddress addr;
        bool isTls;
    };

    /**
     * Puts opened connection into the cache.
     * @param addr Address of connected peer and its TLS status.
     * @param socket Socket associated with an opened connection.
     */
    void put(ConnectionInfo addr, std::unique_ptr<AbstractStreamSocket> socket);

    /**
     * Gets an opened connection from the cache if exists.
     * @param info Address of a peer and its TLS status.
     * @param handler Callback function that is called when the cache lookup completes.
     */
    void take(
        const ConnectionInfo& info,
        nx::utils::MoveOnlyFunc<void(std::unique_ptr<AbstractStreamSocket>)> handler);

    /**
     * Returns the number of elements in the cache.
     * @return Number of elements in the cache.
     */
    size_t size() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::network
