// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/vms/client/core/network/remote_connection_factory.h>

namespace nx::vms::client::core {

class RemoteConnectionFactoryCache
{
private:
    RemoteConnectionFactoryCache() = delete;

public:
    /** Load data from the cache to the connection context. */
    static bool fillContext(std::shared_ptr<RemoteConnectionFactory::Context> context);

    /** Remove cached data from context. */
    static void restoreContext(
        std::shared_ptr<RemoteConnectionFactory::Context> context,
        const LogonData& logonData);

    /** Remove connection cache entry for cloud system. */
    static void clearForCloudId(const QString& cloudId);

    /** Remove connection cache entry for user. */
    static void clearForUser(const std::string& username);

    /** Cleanup cache from outdated entries where token was not updated for more than a month). */
    static void cleanupExpired();

    /**
     * Setup cache updates for the connection. Save cache info immediately if fast connect is used
     * and start periodic cache updates/cleanups.
     */
    static void startWatchingConnection(
        RemoteConnectionPtr connection,
        std::shared_ptr<RemoteConnectionFactoryContext> context,
        bool usedFastConnect);
};

} // namespace nx::vms::client::core
