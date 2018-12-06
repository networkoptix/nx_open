#include "secure_settings.h"

#include <QtCore/QCoreApplication>

#include <nx/utils/property_storage/qsettings_backend.h>
#if defined(USE_QT_KEYCHAIN)
    #include "keychain_property_storage_backend.h"
#endif

namespace nx::vms::client::core {

SecureSettings::SecureSettings():
    nx::utils::property_storage::Storage(
        #if defined(USE_QT_KEYCHAIN)
            new KeychainBackend(QCoreApplication::applicationName()))
        #else
            new nx::utils::property_storage::QSettingsBackend(
                new QSettings(), "client_core_secure"))
        #endif
{
    load();
}

} // namespace nx::vms::client::core
