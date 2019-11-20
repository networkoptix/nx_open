#include "videowall_license_validator.h"

#include <api/runtime_info_manager.h>
#include <common/common_module.h>
#include <licensing/license.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::desktop::license;

bool VideoWallLicenseValidator::overrideMissingRuntimeInfo(
    const QnLicensePtr& license, QnPeerRuntimeInfo& info) const
{
    if (license->type() != Qn::LC_VideoWall)
        return false;

    const auto& manager = runtimeInfoManager();
    auto commonInfo = manager->items()->getItem(commonModule()->remoteGUID());

    if (commonInfo.data.prematureVideoWallLicenseExpirationDate == 0)
    {
        // Runtime info is not found, but information about expiration date is also missing.
        // Assume that other servers are still waiting for the server with license to come up.
        info = commonInfo;
        return true;
    }

    // Video Wall license should remain valid before prematureVideoWallLicenseExpirationDate, so
    // we need to override runtime info from missing server with runtime info from common module.
    // If we reached the expiration date then just report that runtime info is missing.
    const bool stillValid =
        qnSyncTime->currentMSecsSinceEpoch() < commonInfo.data.prematureVideoWallLicenseExpirationDate;

    if (stillValid)
        info = commonInfo;

    return stillValid;
}
