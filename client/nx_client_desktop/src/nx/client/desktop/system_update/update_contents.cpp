#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <client_core/client_core_module.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/update/update_check.h>
#include <nx/vms/discovery/manager.h>
#include <utils/common/app_info.h>

#include "update_contents.h"

namespace nx {
namespace client {
namespace desktop {

nx::utils::SoftwareVersion UpdateContents::getVersion() const
{
    return nx::utils::SoftwareVersion(info.version);
}

// Check if we can apply this update.
bool UpdateContents::isValid() const
{
    return missingUpdate.empty()
        && !info.version.isEmpty()
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

    for (auto& pkg: updateInfo.packages)
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

QSet<QnUuid> getServersLinkedToCloud(QnCommonModule* commonModule, const QSet<QnUuid>& peers)
{
    QSet<QnUuid> result;

    const auto moduleManager = commonModule->moduleDiscoveryManager();
    if (!moduleManager)
        return result;

    for (const auto& id: peers)
    {
        const auto server = commonModule->resourcePool()->getIncompatibleServerById(id);
        if (!server)
            continue;

        const auto module = moduleManager->getModule(id);
        if (module && !module->cloudSystemId.isEmpty())
            result.insert(id);
    }

    return result;
}

bool checkCloudHost(QnCommonModule* commonModule, nx::utils::SoftwareVersion targetVersion, QString cloudUrl, const QSet<QnUuid>& peers)
{
    NX_ASSERT(commonModule);
    /* Ignore cloud host for versions lower than 3.0. */
    static const nx::utils::SoftwareVersion kCloudRequiredVersion(3, 0);

    if (targetVersion < kCloudRequiredVersion)
        return true;

    /* Update is allowed if either target version has the same cloud host or
       there are no servers linked to the cloud in the system. */
    if (cloudUrl == nx::network::SocketGlobals::cloud().cloudHost())
        return true;

    const bool isBoundToCloud = !commonModule->globalSettings()->cloudSystemId().isEmpty();
    if (isBoundToCloud)
        return false;

    const auto serversLinkedToCloud = getServersLinkedToCloud(commonModule, peers);
    return serversLinkedToCloud.isEmpty();
}

} // namespace desktop
} // namespace client
} // namespace nx
