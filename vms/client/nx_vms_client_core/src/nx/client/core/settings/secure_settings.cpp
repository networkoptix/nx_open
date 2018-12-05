#include "secure_settings.h"

#include "keychain_property_storage_backend.h"

#include <nx/utils/app_info.h>

namespace nx::vms::client::core {

using nx::utils::AppInfo;

SecureSettings::SecureSettings():
    nx::utils::property_storage::Storage(new KeychainBackend(
        AppInfo::organizationName() + " " + AppInfo::productNameLong()))
{
    load();
}

} // namespace nx::vms::client::core
