// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_license_validator.h"

#include <api/runtime_info_manager.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <licensing/license.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/common/system_context.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::desktop::license;

bool VideoWallLicenseValidator::overrideMissingRuntimeInfo(
    const QnLicensePtr& license, QnPeerRuntimeInfo& info) const
{
    if (license->type() != Qn::LC_VideoWall)
        return false;

    const auto currentSession = qnClientCoreModule->networkModule()->session();
    if (!currentSession)
        return false;

    const auto& manager = qnClientCoreModule->runtimeInfoManager();
    auto commonInfo =
        manager->items()->getItem(currentSession->connection()->moduleInformation().id);

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
