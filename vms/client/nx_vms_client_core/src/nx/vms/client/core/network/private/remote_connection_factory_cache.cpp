// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_factory_cache.h"

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <nx/reflect/json.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::core;

namespace nx::vms::client::core {

// Cached data stored for each cloud system. Used to speedup connection process.
struct NX_VMS_CLIENT_CORE_API RemoteConnectionContextData
{
    std::vector<nx::vms::api::ServerInformationV1> serversInfo;
    nx::Uuid expectedServerId;
    std::string username;
    std::string bearerTokenValue;
    std::chrono::microseconds sessionTokenExpirationTime{};
};
NX_REFLECTION_INSTRUMENT(RemoteConnectionContextData, (serversInfo)(expectedServerId)(username)(bearerTokenValue)(sessionTokenExpirationTime));

namespace {

void saveCacheInfo(
    RemoteConnectionPtr connection,
    const std::vector<nx::vms::api::ServerInformationV1>& servers)
{
    const auto cloudId = connection->moduleInformation().cloudSystemId;

    const std::string cacheStr =
        nx::vms::client::core::appContext()->coreSettings()->systemAuthenticationCache(
            cloudId);

    RemoteConnectionContextData customData;

    const auto serverIt = std::find_if(servers.begin(), servers.end(),
        [&connection](const auto& server)
        {
            return server.id == connection->moduleInformation().id;
        });

    if (serverIt == servers.end())
        return;

    connection->updateModuleInformation(serverIt->getModuleInformation());
    customData.expectedServerId = serverIt->id;
    customData.serversInfo = servers;
    customData.username = connection->credentials().username;
    customData.bearerTokenValue = connection->credentials().authToken.value;

    if (connection->sessionTokenExpirationTime())
        customData.sessionTokenExpirationTime = *connection->sessionTokenExpirationTime();

    nx::vms::client::core::appContext()->coreSettings()->setSystemAuthenticationCache(
        cloudId,
        nx::reflect::json::serialize(customData));
}

// Request servers info and save it to the cache.
void requestCacheInfoUpdate(RemoteConnectionPtr connection)
{
    connection->serverApi()->getServersInfo(
        [connection](
            bool success,
            rest::Handle /*handle*/,
            const rest::ErrorOrData<std::vector<nx::vms::api::ServerInformationV1>>& response)
        {
            if (!success)
                return;

            auto servers = std::get_if<std::vector<nx::vms::api::ServerInformationV1>>(&response);
            if (!servers)
                return;

            saveCacheInfo(connection, *servers);
        },
        nx::vms::client::core::appContext()->thread());
}

} // namespace

bool RemoteConnectionFactoryCache::fillContext(
    std::shared_ptr<RemoteConnectionFactory::Context> context)
{
    using namespace std::chrono;

    RemoteConnectionContextData customData;

    if (!nx::reflect::json::deserialize(context->logonData.authCacheData, &customData))
    {
        NX_DEBUG(NX_SCOPE_TAG, "Failed to deserialize connection cache.");
        return false;
    }

    // Reuse server list from the previous connection.
    const auto serverIt = std::find_if(customData.serversInfo.begin(), customData.serversInfo.end(),
        [&customData](const auto& server)
        {
            return server.id == customData.expectedServerId;
        });

    if (serverIt == customData.serversInfo.end())
        return false;

    // Make sure that we check the server certificate.
    if (serverIt->certificatePem.empty())
        return false;

    context->logonData.expectedServerId = customData.expectedServerId;
    context->serversInfo = customData.serversInfo;

    context->handshakeCertificateChain =
        nx::network::ssl::Certificate::parse(serverIt->certificatePem);
    context->moduleInformation = serverIt->getModuleInformation();

    context->logonData.expectedServerVersion = context->moduleInformation.version;
    context->logonData.credentials.username = customData.username;

    context->logonData.credentials.authToken =
        nx::network::http::BearerAuthToken(customData.bearerTokenValue);
    if (customData.sessionTokenExpirationTime.count() != 0)
        context->sessionTokenExpirationTime = customData.sessionTokenExpirationTime;

    // The logonData.accessToken is supplied from push IPC storage.
    // If it is newer than the cached token - use the newer one.
    if (!context->logonData.accessToken.empty()
        && context->logonData.tokenExpiresAt > context->sessionTokenExpirationTime.value_or(0s))
    {
        context->logonData.credentials.authToken =
            nx::network::http::BearerAuthToken(context->logonData.accessToken);
        context->sessionTokenExpirationTime = context->logonData.tokenExpiresAt;
    }

    return true;
}

void RemoteConnectionFactoryCache::restoreContext(
    std::shared_ptr<RemoteConnectionFactory::Context> context,
    const LogonData& logonData)
{
    context->resetError();
    context->logonData = logonData;
    context->serversInfo.clear();
    context->sessionTokenExpirationTime = std::nullopt;
}

void RemoteConnectionFactoryCache::clearForCloudId(const QString& cloudId)
{
    nx::vms::client::core::appContext()->coreSettings()->setSystemAuthenticationCache(cloudId, {});
}

void RemoteConnectionFactoryCache::cleanupExpired()
{
    using namespace std::chrono;

    static constexpr auto kRemoveOutdatedCacheThreshold = days{30};

    const microseconds nowTime = qnSyncTime->currentTimePoint();

    auto cacheData =
        nx::vms::client::core::appContext()->coreSettings()->systemAuthenticationCacheData();
    std::erase_if(cacheData,
        [&nowTime](const auto& item) {
            auto const& [key, value] = item;

            RemoteConnectionContextData customData;
            if (!nx::reflect::json::deserialize(value, &customData))
                return true; //< Remove invalid cache.

            const auto daysSinceExpired =
                duration_cast<days>(nowTime - customData.sessionTokenExpirationTime);

            return daysSinceExpired > kRemoveOutdatedCacheThreshold;
        });
    nx::vms::client::core::appContext()->coreSettings()->systemAuthenticationCacheData = cacheData;
}

void RemoteConnectionFactoryCache::startWatchingConnection(
    RemoteConnectionPtr connection,
    std::shared_ptr<RemoteConnectionFactoryContext> context,
    bool usedFastConnect)
{
    using namespace std::chrono;

    static constexpr auto kCacheUpdateInterval = 1min;

    // Setup cache update timer.
    auto updateTimer = std::make_unique<QTimer>();
    updateTimer->setInterval(kCacheUpdateInterval);

    auto updateFunc =
        [remoteConnection = std::weak_ptr<RemoteConnection>(connection)]()
        {
            auto connection = remoteConnection.lock();;
            if (!connection)
                return;

            requestCacheInfoUpdate(connection);
        };

    QObject::connect(updateTimer.get(), &QTimer::timeout, updateFunc);

    updateTimer->start();

    auto timerPointer = updateTimer.get();

    // Connection might get destroyed in another thread, but we need to delete timer in the same
    // thread the timer was created.
    QObject::connect(connection.get(), &QObject::destroyed, timerPointer,
        [updateTimer = std::move(updateTimer)]{ }, Qt::QueuedConnection);

    cleanupExpired();

    if (usedFastConnect)
        updateFunc();
    else
        saveCacheInfo(connection, context->serversInfo);
}

} // namespace nx::vms::client::core
