#include "secure_settings.h"

#include <QtCore/QCoreApplication>

#include <nx/fusion/model_functions.h>

#include "keychain_property_storage_backend.h"

namespace nx::client::core {

SecureSettings::SecureSettings():
    Storage(new KeychainBackend(QCoreApplication::applicationName()))
{
    load();
}

} // namespace nx::client::core
