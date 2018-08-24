#include "media_paths_filter_config.h"
#include "platform/platform_abstraction.h"
#include "media_server/serverutil.h"
#include "media_server/media_server_module.h"
#include "media_server/settings.h"

namespace nx {
namespace mediaserver {
namespace fs {
namespace media_paths {

FilterConfig FilterConfig::createDefault(bool includeNonHdd, const nx::mediaserver::Settings* settings)
{
    FilterConfig result;

    result.isMultipleInstancesAllowed =
        static_cast<bool>(settings->enableMultipleInstances());
    result.isRemovableDrivesAllowed = includeNonHdd
        ? true
        : static_cast<bool>(settings->allowRemovableStorages());
    result.isNetworkDrivesAllowed = includeNonHdd;

    result.partitions = ((QnPlatformMonitor *)qnPlatform->monitor())->totalPartitionSpaceInfo(
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
} // namespace mediaserver
} // namespace nx
