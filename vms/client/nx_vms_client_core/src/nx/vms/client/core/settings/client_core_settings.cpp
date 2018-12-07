#include "client_core_settings.h"

#include <QtCore/QCoreApplication>

//#include "keychain_property_storage_backend.h"

#include <nx/utils/app_info.h>
#include <nx/utils/property_storage/qsettings_backend.h>

namespace nx::vms::client::core {

namespace {

static const QString kGroupName("client_core");

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
    Storage(new utils::property_storage::QSettingsBackend(createSettings()))
{
    const QString serviceName = AppInfo::organizationName() + " " + AppInfo::productNameLong();

    load();
}

} // namespace nx::vms::client::core
