#include "system_helpers.h"

#include <helpers/url_helper.h>
#include <nx/network/socket_global.h>
#include <client_core/client_core_settings.h>

namespace nx {
namespace client {
namespace core {
namespace helpers {

void storeConnection(const QnUuid& localSystemId, const QString& systemName, const QUrl& url)
{
    const auto cleanUrl = QnUrlHelper(url).cleanUrl();

    auto connections = qnClientCoreSettings->recentLocalConnections();

    auto& data = connections[localSystemId];
    data.systemName = systemName;
    data.urls.removeOne(cleanUrl);
    data.urls.prepend(cleanUrl);

    qnClientCoreSettings->setRecentLocalConnections(connections);
}

void removeConnection(const QnUuid& localSystemId, const QUrl& url)
{
    auto connections = qnClientCoreSettings->recentLocalConnections();

    if (!url.isValid())
    {
        connections.remove(localSystemId);
    }
    else
    {
        auto it = connections.find(localSystemId);

        if (it != connections.end())
            it->urls.removeOne(QnUrlHelper(url).cleanUrl());
    }

    qnClientCoreSettings->setRecentLocalConnections(connections);
}

void storeCredentials(const QnUuid& localSystemId, const QnCredentials& credentials)
{
    auto credentialsHash = qnClientCoreSettings->systemAuthenticationData();
    auto& credentialsList = credentialsHash[localSystemId];

    auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
        [&credentials](const QnCredentials& other)
        {
            return credentials.user == other.user;
        });

    if (it != credentialsList.end())
        credentialsList.erase(it);

    credentialsList.prepend(credentials);

    qnClientCoreSettings->setSystemAuthenticationData(credentialsHash);
}

void removeCredentials(const QnUuid& localSystemId, const QString& userName)
{
    auto credentialsHash = qnClientCoreSettings->systemAuthenticationData();

    if (userName.isEmpty())
    {
        credentialsHash.remove(localSystemId);
    }
    else
    {
        auto& credentialsList = credentialsHash[localSystemId];

        auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
            [&userName](const QnCredentials& other)
            {
                return userName == other.user;
            });

        if (it != credentialsList.end())
            credentialsList.erase(it);
    }

    qnClientCoreSettings->setSystemAuthenticationData(credentialsHash);
}

QnCredentials getCredentials(const QnUuid& localSystemId, const QString& userName)
{
    auto credentialsHash = qnClientCoreSettings->systemAuthenticationData();
    auto& credentialsList = credentialsHash[localSystemId];

    auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
        [&userName](const QnCredentials& other)
        {
            return userName == other.user;
        });

    if (it != credentialsList.end())
        return *it;

    return QnCredentials();
}

bool hasCredentials(const QnUuid& localSystemId)
{
    const auto& credentialsHash = qnClientCoreSettings->systemAuthenticationData();
    return !credentialsHash[localSystemId].isEmpty();
}

} // namespace helpers
} // namespace core
} // namespace client
} // namespace nx
