#include "videowall_license_validator.h"

#include <chrono>

#include <api/runtime_info_manager.h>
#include <common/common_module.h>
#include <licensing/license.h>
#include <utils/common/synctime.h>

using namespace std::chrono;
using namespace nx::vms::client::desktop::license;

namespace {

constexpr auto kVideoWallOverflowTimeout = 7 * 24h;

} // namespace

bool VideoWallLicenseValidator::overrideMissingRuntimeInfo(const QnLicensePtr& license, QnPeerRuntimeInfo& info) const
{
    if (license->type() != Qn::LC_VideoWall)
        return false;

    const auto& manager = runtimeInfoManager();
    auto commonInfo = manager->items()->getItem(commonModule()->remoteGUID());

    if (commonInfo.data.prematureVideoWallLicenseExperationDate == 0)
        return false;

    // Video Wall license should remain valid within kVideoWallOverflowTimeout since the server went offline,
    // so override runtime info from missing server with runtime info from common module.
    // If we are outside of the allowed timeframe then just report the missing runtime info.
    const auto delta = qnSyncTime->currentMSecsSinceEpoch() - commonInfo.data.prematureVideoWallLicenseExperationDate;
    const bool stillValid = delta < duration_cast<milliseconds>(kVideoWallOverflowTimeout).count();

    if (stillValid)
        info = commonInfo;

    return stillValid;
}
