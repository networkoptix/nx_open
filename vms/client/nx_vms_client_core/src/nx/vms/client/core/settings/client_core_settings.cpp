#include "client_core_settings.h"

#include <QtCore/QCoreApplication>

#include <utils/crypt/symmetrical.h>

#include <nx/client/core/settings/keychain_property_storage_backend.h>

#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/property_storage/qsettings_backend.h>

namespace nx::vms::client::core {

namespace {

static const QString kGroupName("client_core");
static const QString kSecureKeyPropertyName("key");

QSettings* createSettings()
{
    return new QSettings(
        QSettings::NativeFormat,
        QSettings::UserScope,
        QCoreApplication::organizationName(),
        QCoreApplication::applicationName());
}

} // namespace

using nx::utils::AppInfo;

Settings::Settings():
    Storage(new utils::property_storage::QSettingsBackend(createSettings(), kGroupName))
{
    const QString serviceName = AppInfo::organizationName() + " " + AppInfo::productNameLong();
    KeychainBackend keychain(serviceName);

    QByteArray key = keychain.readValue(kSecureKeyPropertyName).toUtf8();
    if (key.isEmpty())
    {
        key = nx::utils::generateAesExtraKey();
        if (!keychain.writeValue(kSecureKeyPropertyName, QString::fromUtf8(key)))
        {
            NX_WARNING(this, "Keychain is not available, using predefined key");
            key = {};
        }
    }

    for (auto property: properties())
    {
        if (auto secured = dynamic_cast<Secured*>(property))
            secured->setSecurityKey(key);
    }

    load();
}

} // namespace nx::vms::client::core
