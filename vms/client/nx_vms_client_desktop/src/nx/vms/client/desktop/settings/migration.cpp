#include "migration.h"

#include <client/client_settings.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace nx::vms::client::desktop::settings {

namespace {

static const QString kXorKey = "ItIsAGoodDayToDie";
static const QString kCustomConnectionsKey = "AppServerConnections";
static const QString kUrlTag = "url";
static const QString kLocalId = "localId";
static const QString kPasswordTag = "pwd";

void migratePasswordFromCustomConnections()
{
    auto authData = core::settings()->systemAuthenticationData();

    auto settings = qnSettings->rawSettings();
    settings->beginGroup(kCustomConnectionsKey);
    for (const auto& group: settings->childGroups())
    {
        settings->beginGroup(group);

        const auto password = settings->value(kPasswordTag).toString();
        const auto localSystemId = settings->value(kLocalId).toUuid();

        if (!password.isEmpty() && !localSystemId.isNull())
        {
            const auto decodedPassword = nx::utils::xorDecrypt(password, kXorKey);
            const nx::utils::Url url = settings->value(kUrlTag).toString();
            const auto userName = url.userName();

            nx::vms::common::Credentials credentials(userName, decodedPassword);
            if (credentials.isValid())
            {
                auto& credentialsList = authData[localSystemId];
                if (!credentialsList.contains(credentials))
                    credentialsList.append(credentials);
            }

        }
        settings->remove(kPasswordTag);
        settings->endGroup();
    }
    settings->endGroup();
    settings->sync();

    core::settings()->systemAuthenticationData = authData;
}

}

void migrate()
{
    migratePasswordFromCustomConnections();
}

} // namespace nx::vms::client::desktop::settings
