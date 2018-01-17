#include "system_helpers.h"

#include <helpers/url_helper.h>
#include <nx/network/socket_global.h>
#include <client_core/client_core_settings.h>

#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace core {
namespace helpers {

struct Credentials {};
const utils::log::Tag kCredentialsLogTag(typeid(Credentials));

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

void clearSavedPasswords()
{
    QnClientCoreSettings::SystemAuthenticationDataHash result;

    const auto credentialsHash = qnClientCoreSettings->systemAuthenticationData();
    for (auto it = credentialsHash.begin(); it != credentialsHash.end(); ++it)
    {
        QList<QnEncodedCredentials> credentials;
        for (const auto& currentCredential: it.value())
            credentials.append(QnEncodedCredentials(currentCredential.user, QString()));
        result[it.key()] = credentials;
    }
    qnClientCoreSettings->setSystemAuthenticationData(result);
    qnClientCoreSettings->save();
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

void storeCredentials(const QnUuid& localSystemId, const QnEncodedCredentials& credentials)
{
    NX_DEBUG(kCredentialsLogTag, lm("Saving credentials of %1 to the system %2")
        .args(credentials.user, localSystemId));

    NX_DEBUG(kCredentialsLogTag, credentials.decodedPassword().isEmpty()
        ? "Password is empty"
        : "Password is filled");

    auto credentialsHash = qnClientCoreSettings->systemAuthenticationData();
    auto& credentialsList = credentialsHash[localSystemId];

    auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
        [&credentials](const QnEncodedCredentials& other)
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
    NX_DEBUG(kCredentialsLogTag, lm("Removing credentials of %1 to the system %2")
        .args(userName, localSystemId));

    auto credentialsHash = qnClientCoreSettings->systemAuthenticationData();

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

    qnClientCoreSettings->setSystemAuthenticationData(credentialsHash);
}

QnEncodedCredentials getCredentials(const QnUuid& localSystemId, const QString& userName)
{
    auto credentialsHash = qnClientCoreSettings->systemAuthenticationData();
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
    const auto& credentialsHash = qnClientCoreSettings->systemAuthenticationData();
    return !credentialsHash.value(localSystemId).isEmpty();
}

} // namespace helpers
} // namespace core
} // namespace client
} // namespace nx
