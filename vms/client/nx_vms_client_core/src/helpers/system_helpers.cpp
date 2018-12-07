#include "system_helpers.h"

#include <nx/vms/client/core/settings/client_core_settings.h>
#include <client_core/client_core_settings.h>

#include <nx/network/socket_global.h>

#include <nx/utils/uuid.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::core {
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
    Settings::SystemAuthenticationDataHash result;

    const auto credentialsHash = settings()->systemAuthenticationData();
    for (auto it = credentialsHash.begin(); it != credentialsHash.end(); ++it)
    {
        QList<nx::vms::common::Credentials> credentials;
        for (const auto& currentCredential: it.value())
            credentials.append({currentCredential.user, QString()});
        result[it.key()] = credentials;
    }
    settings()->systemAuthenticationData = result;
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

void storeCredentials(const QnUuid& localSystemId, const nx::vms::common::Credentials& credentials)
{
    NX_DEBUG(kCredentialsLogTag, lm("Saving credentials of %1 to the system %2")
        .args(credentials.user, localSystemId));

    NX_DEBUG(kCredentialsLogTag, credentials.password.isEmpty()
        ? "Password is empty"
        : "Password is filled");

    auto credentialsHash = settings()->systemAuthenticationData();
    auto& credentialsList = credentialsHash[localSystemId];

    auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
        [&credentials](const auto& other)
        {
            return credentials.user == other.user;
        });

    if (it != credentialsList.end())
        credentialsList.erase(it);

    credentialsList.prepend(credentials);

    settings()->systemAuthenticationData = credentialsHash;
}

void removeCredentials(const QnUuid& localSystemId, const QString& userName)
{
    NX_DEBUG(kCredentialsLogTag, lm("Removing credentials of %1 to the system %2")
        .args(userName, localSystemId));

    auto credentialsHash = settings()->systemAuthenticationData();

    if (userName.isEmpty())
    {
        credentialsHash.remove(localSystemId);
    }
    else
    {
        auto& credentialsList = credentialsHash[localSystemId];

        auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
            [&userName](const auto& other)
            {
                return userName == other.user;
            });

        if (it != credentialsList.end())
            credentialsList.erase(it);
    }

    settings()->systemAuthenticationData = credentialsHash;
}

nx::vms::common::Credentials getCredentials(const QnUuid& localSystemId, const QString& userName)
{
    auto credentialsHash = settings()->systemAuthenticationData();
    auto& credentialsList = credentialsHash[localSystemId];

    const auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
        [&userName](const auto& other)
        {
            return userName == other.user;
        });

    if (it != credentialsList.end())
        return *it;

    return {};
}

bool hasCredentials(const QnUuid& localSystemId)
{
    const auto& credentialsHash = settings()->systemAuthenticationData();
    return !credentialsHash.value(localSystemId).isEmpty();
}

} // namespace helpers
} // namespace nx::vms::client::core
