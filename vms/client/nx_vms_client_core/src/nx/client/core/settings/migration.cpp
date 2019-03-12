#include "migration.h"

#include <nx/vms/client/core/settings/client_core_settings.h>
#include <client_core/client_core_settings.h>

#include <nx/utils/range_adapters.h>

#include <helpers/system_helpers.h>

namespace nx::vms::client::core::settings_migration {

namespace {

void migrateAuthenticationData()
{
    const auto migrateEncoded =
        [](const EncodedCredentials& encoded) -> nx::vms::common::Credentials
        {
             return {encoded.user, encoded.password.decoded()};
        };

    const auto oldData = qnClientCoreSettings->systemAuthenticationData();
    qnClientCoreSettings->setSystemAuthenticationData({});

    if (settings()->systemAuthenticationData().isEmpty())
    {
        Settings::SystemAuthenticationDataHash data;
        for (auto [systemId, credentialsList]: nx::utils::constKeyValueRange(oldData))
        {
            QList<nx::vms::common::Credentials> credentials;
            std::transform(credentialsList.cbegin(), credentialsList.cend(),
                std::back_inserter(credentials),
                migrateEncoded);
            data.insert(systemId, credentials);
        }
        settings()->systemAuthenticationData = data;
    }

    const EncodedCredentials oldCloudCredentials(
        qnClientCoreSettings->cloudLogin(), qnClientCoreSettings->cloudPassword());
    qnClientCoreSettings->setCloudLogin(QString());
    qnClientCoreSettings->setCloudPassword(QString());

    if (settings()->cloudCredentials().isEmpty())
        helpers::saveCloudCredentials(migrateEncoded(oldCloudCredentials));
}

} // namespace

void migrate()
{
    migrateAuthenticationData();
}

} // namespace nx::vms::client::core::settings_migration
