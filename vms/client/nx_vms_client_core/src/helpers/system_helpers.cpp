// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_helpers.h"

#include <client_core/client_core_settings.h>
#include <nx/network/address_resolver.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace nx::vms::client::core {
namespace helpers {

namespace {

static constexpr int kMaxStoredPreferredCloudServers = 100;

} // namespace

struct Credentials {};
const utils::log::Tag kCredentialsLogTag(typeid(Credentials));
const char* const kCloudUrlSchemeName = "cloud";

void storeConnection(const QnUuid& localSystemId, const QString& systemName, const nx::utils::Url& url)
{
    NX_VERBOSE(NX_SCOPE_TAG, "Storing recent system connection id: %1, url: %2",
        localSystemId, url);

    const auto cleanUrl = url.cleanUrl();

    auto connections = qnClientCoreSettings->recentLocalConnections();

    auto& data = connections[localSystemId];
    data.systemName = systemName;
    data.urls.removeOne(cleanUrl);
    data.urls.prepend(cleanUrl);

    qnClientCoreSettings->setRecentLocalConnections(connections);
}

void removeConnection(const QnUuid& localSystemId, const nx::utils::Url& url)
{
    NX_VERBOSE(NX_SCOPE_TAG, "Removing recent system connection id: %1, url: %2",
        localSystemId, url);

    auto connections = qnClientCoreSettings->recentLocalConnections();

    if (!url.isValid())
    {
        connections.remove(localSystemId);
    }
    else
    {
        auto it = connections.find(localSystemId);

        if (it != connections.end())
            it->urls.removeOne(url.cleanUrl());
    }

    qnClientCoreSettings->setRecentLocalConnections(connections);
}

CloudAuthData loadCloudCredentials()
{
    nx::network::http::Credentials credentials = settings()->cloudCredentials();

    CloudAuthData authData;
    authData.credentials.authToken.type = nx::network::http::AuthTokenType::bearer;
    authData.credentials.username = std::move(credentials.username);
    authData.refreshToken = std::move(credentials.authToken.value);

    return authData;
}

void saveCloudCredentials(const CloudAuthData& authData)
{
    // Save username so it can be restored for future connections to cloud.
    const std::string username = authData.credentials.username.empty()
        ? settings()->cloudCredentials().user
        : authData.credentials.username;

    settings()->cloudCredentials = nx::network::http::Credentials(
        username, nx::network::http::BearerAuthToken(authData.refreshToken));
}

std::optional<QnUuid> preferredCloudServer(const QString& systemId)
{
    const auto preferredServers = settings()->preferredCloudServers();
    const auto iter = std::find_if(preferredServers.cbegin(), preferredServers.cend(),
        [&systemId](const auto& item) { return item.systemId == systemId; });

    if (iter == preferredServers.cend())
        return std::nullopt;

    return iter->serverId;
}

void savePreferredCloudServer(const QString& systemId, const QnUuid& serverId)
{
    auto preferredServers = settings()->preferredCloudServers();
    const auto iter = std::find_if(preferredServers.begin(), preferredServers.end(),
        [&systemId](const auto& item) { return item.systemId == systemId; });

    if (iter != preferredServers.end())
        preferredServers.erase(iter);

    NX_DEBUG(typeid(Settings), "Setting server %1 as preferred for cloud system %2",
        serverId.toSimpleString(), systemId);

    preferredServers.push_back({systemId, serverId});

    while (preferredServers.size() > kMaxStoredPreferredCloudServers)
        preferredServers.pop_front();

    settings()->preferredCloudServers = preferredServers;
}

QString serverCloudHost(const QString& systemId, const QnUuid& serverId)
{
    return QString("%1.%2").arg(serverId.toSimpleString(), systemId);
}

bool isCloudUrl(const nx::utils::Url& url)
{
    return url.scheme() == kCloudUrlSchemeName
        || nx::network::SocketGlobals::addressResolver().isCloudHostname(url.host());
}

} // namespace helpers
} // namespace nx::vms::client::core
