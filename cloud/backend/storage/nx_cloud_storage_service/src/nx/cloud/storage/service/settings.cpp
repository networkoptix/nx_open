#include "settings.h"

#include <nx/utils/app_info.h>

namespace nx::cloud::storage::service {

static constexpr char kModuleName[] = "nx_cloud_storage_service";

Settings::Settings():
    base_type(
        nx::utils::AppInfo::organizationNameForSettings(),
        kModuleName,
        kModuleName)
{
}

void Settings::loadSettings()
{
    // TODO
}

} // namespace nx::cloud::storage::service
