// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <api/runtime_info_manager.h>
#include <client/client_runtime_settings.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/videowall/videowall_online_screens_watcher.h>
#include <nx/vms/license/usage_helper.h>

namespace nx::vms::client::desktop::license {

bool isVideoWallLicenseValid(const QnResourcePtr& resource)
{
    if (!NX_ASSERT(resource) || !NX_ASSERT(qnRuntime->isVideoWallMode()))
        return false;

    const auto systemContext = SystemContext::fromResource(resource);
    const auto helper = systemContext->videoWallLicenseUsageHelper();

    if (!NX_ASSERT(helper))
        return false;

    if (helper->isValid())
        return true;

    const auto localInfo = systemContext->runtimeInfoManager()->localInfo();
    NX_ASSERT(localInfo.data.peer.peerType == nx::vms::api::PeerType::videowallClient);
    nx::Uuid currentScreenId = localInfo.data.videoWallInstanceGuid;

    // Gather all online screen ids.
    // The order of screens should be the same across all client instances.
    const auto videoWallOnlineScreensWatcher = systemContext->videoWallOnlineScreensWatcher();
    const auto onlineScreens = videoWallOnlineScreensWatcher->onlineScreens();

    const int allowedLicenses = helper->totalLicenses(Qn::LC_VideoWall);

    // Calculate the number of screens that should show invalid license overlay.
    using Helper = nx::vms::license::VideoWallLicenseUsageHelper;
    size_t screensToDisable = 0;
    while (screensToDisable < onlineScreens.size()
        && Helper::licensesForScreens(onlineScreens.size() - screensToDisable) > allowedLicenses)
    {
        ++screensToDisable;
    }

    if (screensToDisable > 0)
    {
        // If current screen falls into the range - report invalid license.
        const size_t i = std::distance(onlineScreens.begin(), onlineScreens.find(currentScreenId));
        if (i < screensToDisable)
            return false;
    }

    return true;
}

} // namespace nx::vms::client::desktop::license
