// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "credentials_manager.h"

#include <nx/network/http/auth_tools.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace nx::vms::client::core {

using Credentials = nx::network::http::Credentials;
using SerializableCredentials = nx::network::http::SerializableCredentials;
using SerializableCredentialsList = std::vector<SerializableCredentials>;

namespace {

auto caseInsensitiveUserMatch(std::string user)
{
    return
        [user = QString::fromStdString(user)](const SerializableCredentials& creds)
        {
            return user.compare(QString::fromStdString(creds.user), Qt::CaseInsensitive) == 0;
        };
}

auto findUser(SerializableCredentialsList& credentialsList, const std::string& user)
{
    return std::find_if(credentialsList.begin(), credentialsList.end(),
        caseInsensitiveUserMatch(user));
}

auto findUser(const SerializableCredentialsList& credentialsList, const std::string& user)
{
    return std::find_if(credentialsList.begin(), credentialsList.end(),
        caseInsensitiveUserMatch(user));
}

} // namespace

CredentialsManager::CredentialsManager(QObject* parent):
    QObject(parent)
{
    connect(&settings()->systemAuthenticationData,
        &Settings::BaseProperty::changed,
        this,
        &CredentialsManager::storedCredentialsChanged);
}

void CredentialsManager::storeCredentials(
    const QnUuid& localSystemId,
    const Credentials& credentials)
{
    NX_ASSERT(!credentials.username.empty());

    SerializableCredentials serialized(credentials);
    NX_DEBUG(NX_SCOPE_TAG,
        "Saving credentials of %1 to the system %2",
        serialized.user,
        localSystemId);

    auto credentialsHash = settings()->systemAuthenticationData();
    auto& credentialsList = credentialsHash[localSystemId];

    auto it = findUser(credentialsList, serialized.user);
    if (it != credentialsList.end())
        credentialsList.erase(it);

    credentialsList.insert(credentialsList.begin(), serialized);

    settings()->systemAuthenticationData = credentialsHash;
}

void CredentialsManager::removeCredentials(const QnUuid& localSystemId, const std::string& user)
{
    NX_DEBUG(NX_SCOPE_TAG, "Removing credentials of %1 to the system %2", user, localSystemId);

    auto systemAuthenticationData = settings()->systemAuthenticationData();
    auto credentialsIter = systemAuthenticationData.find(localSystemId);
    if (credentialsIter == systemAuthenticationData.end())
        return;

    SerializableCredentialsList& credentialsList = credentialsIter->second;

    auto it = findUser(credentialsList, user);
    if (it != credentialsList.end())
    {
        credentialsList.erase(it);
        if (credentialsList.empty())
            systemAuthenticationData.erase(credentialsIter);

        settings()->systemAuthenticationData = systemAuthenticationData;
    }
}

void CredentialsManager::removeCredentials(const QnUuid& localSystemId)
{
    NX_DEBUG(NX_SCOPE_TAG, "Removing all credentials for the system %1", localSystemId);

    auto systemAuthenticationData = settings()->systemAuthenticationData();
    auto credentialsIter = systemAuthenticationData.find(localSystemId);
    if (credentialsIter == systemAuthenticationData.end())
        return;

    systemAuthenticationData.erase(credentialsIter);
    settings()->systemAuthenticationData = systemAuthenticationData;
}

void CredentialsManager::forgetStoredPassword(const QnUuid& localSystemId, const std::string& user)
{
    NX_DEBUG(NX_SCOPE_TAG, "Forget password of %1 to the system %2", user, localSystemId);

    auto systemAuthenticationData = settings()->systemAuthenticationData();
    SerializableCredentialsList& credentialsList = systemAuthenticationData[localSystemId];
    auto it = findUser(credentialsList, user);
    if (it == credentialsList.end())
        return;

    *it = {user};
    settings()->systemAuthenticationData = systemAuthenticationData;
}

void CredentialsManager::forgetStoredPasswords()
{
    NX_DEBUG(NX_SCOPE_TAG, "Forget all passwords");

    Settings::SystemAuthenticationData cleanData;
    const auto systemAuthenticationData = settings()->systemAuthenticationData();
    for (auto [localSystemId, credentialsList]: systemAuthenticationData)
    {
        SerializableCredentialsList cleanCredentials;
        for (const auto& credentials: credentialsList)
            cleanCredentials.push_back(Credentials(credentials.user, {}));
        cleanData.emplace(localSystemId, std::move(cleanCredentials));
    }

    settings()->systemAuthenticationData = cleanData;
}

std::vector<Credentials> CredentialsManager::credentials(const QnUuid& localSystemId)
{
    const auto systemAuthenticationData = settings()->systemAuthenticationData();
    const auto credentialsIter = systemAuthenticationData.find(localSystemId);
    if (credentialsIter == systemAuthenticationData.cend())
        return {};

    const SerializableCredentialsList& credentialsList = credentialsIter->second;

    std::vector<Credentials> result;
    result.reserve(credentialsList.size());
    for (const auto& credentials: credentialsList)
        result.emplace_back(credentials);
    return result;
}

std::optional<Credentials> CredentialsManager::credentials(
    const QnUuid& localSystemId,
    const std::string& user)
{
    const auto systemAuthenticationData = settings()->systemAuthenticationData();
    const auto credentialsIter = systemAuthenticationData.find(localSystemId);
    if (credentialsIter == systemAuthenticationData.cend())
        return {};

    const SerializableCredentialsList& credentialsList = credentialsIter->second;
    const auto it = findUser(credentialsList, user);
    if (it != credentialsList.end())
        return *it;

    return {};
}

} // namespace nx::vms::client::core {