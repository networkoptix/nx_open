#include "migration.h"

#include <nx/client/core/settings/secure_settings.h>
#include <client_core/client_core_settings.h>

namespace nx::vms::client::core::settings_migration {

namespace {

void migrateAuthenticationData()
{
    const auto oldData = qnClientCoreSettings->systemAuthenticationData();
    qnClientCoreSettings->setSystemAuthenticationData({});

    if (secureSettings()->systemAuthenticationData().isEmpty())
        secureSettings()->systemAuthenticationData = oldData;

    const EncodedCredentials oldCloudCredentials(
        qnClientCoreSettings->cloudLogin(), qnClientCoreSettings->cloudPassword());
    qnClientCoreSettings->setCloudLogin(QString());
    qnClientCoreSettings->setCloudPassword(QString());

    if (secureSettings()->cloudCredentials().isEmpty())
        secureSettings()->cloudCredentials = oldCloudCredentials;
}

} // namespace

void migrate()
{
    migrateAuthenticationData();
}

} // namespace nx::vms::client::core::settings_migration
