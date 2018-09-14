#include "system_helpers.h"

#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>
#include <nx/client/core/settings/secure_settings.h>
#include <client_core/client_core_settings.h>

namespace nx {
namespace client {
namespace core {
namespace helpers {

struct Credentials {};
const utils::log::Tag kCredentialsLogTag(typeid(Credentials));

void storeConnection(const QnUuid& localSystemId, const QString& systemName, const nx::utils::Url& url)
{
    const auto cleanUrl = url.cleanUrl();

    auto connections = qnClientCoreSettings->recentLocalConnections();

    auto& data = connections[localSystemId];
    data.systemName = systemName;
    data.urls.removeOne(cleanUrl);
    data.urls.prepend(cleanUrl);

    qnClientCoreSettings->setRecentLocalConnections(connections);
}

void clearSavedPasswords()
{
    QnClientCoreSettings::SystemAuthenticationDataHash result;

    const auto credentialsHash = secureSettings()->systemAuthenticationData();
    for (auto it = credentialsHash.begin(); it != credentialsHash.end(); ++it)
    {
        QList<QnEncodedCredentials> credentials;
        for (const auto& currentCredential: it.value())
            credentials.append(QnEncodedCredentials(currentCredential.user, QString()));
        result[it.key()] = credentials;
    }
    secureSettings()->systemAuthenticationData = result;
}

void removeConnection(const QnUuid& localSystemId, const nx::utils::Url& url)
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
            it->urls.removeOne(url.cleanUrl());
    }

    qnClientCoreSettings->setRecentLocalConnections(connections);
}

void storeCredentials(const QnUuid& localSystemId, const QnEncodedCredentials& credentials)
{
    NX_DEBUG(kCredentialsLogTag, lm("Saving credentials of %1 to the system %2")
        .args(credentials.user, localSystemId));

    NX_DEBUG(kCredentialsLogTag, credentials.decodedPassword().isEmpty()
        ? "Password is empty"
        : "Password is filled");

    auto credentialsHash = secureSettings()->systemAuthenticationData();
    auto& credentialsList = credentialsHash[localSystemId];

    auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
        [&credentials](const QnEncodedCredentials& other)
        {
            return credentials.user == other.user;
        });

    if (it != credentialsList.end())
        credentialsList.erase(it);

    credentialsList.prepend(credentials);

    secureSettings()->systemAuthenticationData = credentialsHash;
}

void removeCredentials(const QnUuid& localSystemId, const QString& userName)
{
    NX_DEBUG(kCredentialsLogTag, lm("Removing credentials of %1 to the system %2")
        .args(userName, localSystemId));

    auto credentialsHash = secureSettings()->systemAuthenticationData();

    if (userName.isEmpty())
    {
        credentialsHash.remove(localSystemId);
    }
    else
    {
        auto& credentialsList = credentialsHash[localSystemId];

        auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
            [&userName](const QnEncodedCredentials& other)
            {
                return userName == other.user;
            });

        if (it != credentialsList.end())
            credentialsList.erase(it);
    }

    secureSettings()->systemAuthenticationData = credentialsHash;
}

QnEncodedCredentials getCredentials(const QnUuid& localSystemId, const QString& userName)
{
    auto credentialsHash = secureSettings()->systemAuthenticationData();
    auto& credentialsList = credentialsHash[localSystemId];

    auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
        [&userName](const QnEncodedCredentials& other)
        {
            return userName == other.user;
        });

    if (it != credentialsList.end())
        return *it;

    return QnEncodedCredentials();
}

bool hasCredentials(const QnUuid& localSystemId)
{
    const auto& credentialsHash = secureSettings()->systemAuthenticationData();
    return !credentialsHash.value(localSystemId).isEmpty();
}

} // namespace helpers
} // namespace core
} // namespace client
} // namespace nx
