#include "secure_settings.h"

#include <QtCore/QCoreApplication>

#include "keychain_property_storage_backend.h"
#include <nx/utils/property_storage/qsettings_backend.h>

namespace nx::client::core {

SecureSettings::SecureSettings():
    nx::utils::property_storage::Storage(new KeychainBackend(QCoreApplication::applicationName()))
{
    load();
}

} // namespace nx::client::core
