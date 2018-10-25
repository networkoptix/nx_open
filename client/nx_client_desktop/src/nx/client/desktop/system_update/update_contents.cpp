#include <utils/common/app_info.h>
#include <nx/update/update_check.h>

#include "update_contents.h"

namespace nx {
namespace client {
namespace desktop {

// Check if we can apply this update.
bool UpdateContents::isValid() const
{
    return missingUpdate.empty()
        && invalidVersion.empty()
        && clientPackage.isValid()
        && error == nx::update::InformationError::noError;
}

nx::update::Package findClientPackage(const nx::update::Information& updateInfo)
{
    // Find client package.
    nx::update::Package result;
    const auto modification = QnAppInfo::applicationPlatformModification();
    auto arch = QnAppInfo::applicationArch();
    auto runtime = nx::vms::api::SystemInformation::currentSystemRuntime().replace(L' ', L'_');

    for(auto& pkg: updateInfo.packages)
    {
        if (pkg.component == nx::update::kComponentClient)
        {
            // Check arch and OS
            if (pkg.arch == arch && pkg.variant == modification)
            {
                result = pkg;
                break;
            }
        }
    }

    return result;
}

} // namespace desktop
} // namespace client
} // namespace nx
