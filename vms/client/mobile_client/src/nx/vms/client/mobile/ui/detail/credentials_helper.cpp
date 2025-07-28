// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "credentials_helper.h"

#include <nx/network/http/auth_tools.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/network/oauth_client.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/mobile/application_context.h>

namespace nx::vms::client::mobile::detail {

CredentialsHelper::CredentialsHelper(QObject* parent):
    base_type(parent)
{
    using SecureProperty = nx::utils::property_storage::SecureProperty<std::string>;
    connect(&appContext()->coreSettings()->digestCloudPassword, &SecureProperty::changed,
        this, &CredentialsHelper::digestCloudPasswordChanged);
}


void CredentialsHelper::removeSavedConnection(
    const QString& systemId,
    const QString& localSystemId,
    const QString& userName)
{
    const auto localId = nx::Uuid::fromStringSafe(localSystemId);

    NX_ASSERT(!localId.isNull());
    if (localId.isNull())
        return;

    core::CredentialsManager::removeCredentials(localId, userName.toStdString());

    if (userName.isEmpty() || core::CredentialsManager::credentials(localId).empty())
    {
        appContext()->coreSettings()->removeRecentConnection(localId);
        appContext()->knownServerConnectionsWatcher()->removeSystem(systemId);
    }
}

void CredentialsHelper::removeSavedCredentials(
    const QString& localSystemId,
    const QString& userName)
{
    if (!NX_ASSERT(!localSystemId.isNull() && !userName.isEmpty()))
        return;

    const auto localId = nx::Uuid::fromStringSafe(localSystemId);
    core::CredentialsManager::removeCredentials(localId, userName.toStdString());
}

void CredentialsHelper::removeSavedAuth(
    const QString& localSystemId,
    const QString& userName)
{
    if (!NX_ASSERT(!localSystemId.isNull() && !userName.isEmpty()))
        return;

    const auto localId = nx::Uuid::fromStringSafe(localSystemId);
    const auto emptyAuthCredentials = nx::network::http::Credentials{userName.toStdString(), {}};
    core::CredentialsManager::storeCredentials(localId, emptyAuthCredentials);
}

void CredentialsHelper::clearSavedPasswords()
{
    core::CredentialsManager::forgetStoredPasswords();
}

core::OauthClient* CredentialsHelper::createOauthClient(
    const QString& token,
    const QString& user,
    bool forced) const
{
    const core::OauthClientType type = token.isEmpty()
        ? core::OauthClientType::loginCloud
        : core::OauthClientType::renewDesktop;

    static constexpr std::chrono::days kRefreshTokenLifetime(180);
    const auto result = new core::OauthClient(type, core::OauthViewType::mobile, /*cloudSystem*/ {},
        kRefreshTokenLifetime);

    result->setCredentials(nx::network::http::Credentials(
        user.toStdString(),
        token.isEmpty()
            ? nx::network::http::AuthToken{}
            : nx::network::http::BearerAuthToken(token.toStdString())));

    connect(result, &core::OauthClient::authDataReady, this,
        [result, forced]()
        {
            const auto authData = result->authData();
            if (authData.empty())
            {
                emit result->cancelled();
                return;
            }

            appContext()->cloudStatusWatcher()->setAuthData(authData, forced
                ? core::CloudStatusWatcher::AuthMode::forced
                : core::CloudStatusWatcher::AuthMode::login);
        });
    return result;
}

bool CredentialsHelper::hasDigestCloudPassword() const
{
    return !appContext()->coreSettings()->digestCloudPassword().empty();
}

} // namespace nx::vms::client::mobile::detail
