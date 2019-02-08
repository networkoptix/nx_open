#include "media_paths_filter_config.h"
#include "platform/platform_abstraction.h"
#include "media_server/serverutil.h"
#include "media_server/media_server_module.h"
#include "media_server/settings.h"

namespace nx {
namespace vms::server {
namespace fs {
namespace media_paths {

FilterConfig FilterConfig::createDefault(
    QnPlatformAbstraction* platform,
    bool includeNonHdd,
    const nx::vms::server::Settings* settings)
{
    FilterConfig result;

    result.isMultipleInstancesAllowed =
        static_cast<bool>(settings->enableMultipleInstances());
    result.isRemovableDrivesAllowed = includeNonHdd
        ? true
        : static_cast<bool>(settings->allowRemovableStorages());
    result.isNetworkDrivesAllowed = includeNonHdd;

    result.partitions = ((QnPlatformMonitor *)platform->monitor())->totalPartitionSpaceInfo(
        QnPlatformMonitor::LocalDiskPartition
        | QnPlatformMonitor::NetworkPartition
        | QnPlatformMonitor::RemovableDiskPartition);

    result.dataDirectory = settings->dataDir();
    result.mediaFolderName = QnAppInfo::mediaFolderName();
    result.serverUuid = QnUuid(settings->serverGuid());

#if defined (Q_OS_WIN)
    result.isWindows = true;
#endif

    return result;
}

} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx
