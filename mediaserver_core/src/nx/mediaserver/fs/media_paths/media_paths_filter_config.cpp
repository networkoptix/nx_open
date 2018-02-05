#include "media_paths_filter_config.h"
#include "platform/platform_abstraction.h"
#include "media_server/serverutil.h"
#include "media_server/media_server_module.h"
#include "media_server/settings.h"

namespace nx {
namespace mediaserver {
namespace fs {
namespace media_paths {

FilterConfig FilterConfig::createDefault(bool includeNonHdd)
{
    FilterConfig result;

    const auto settings = qnServerModule->roSettings();
    result.isMultipleInstancesAllowed =
        static_cast<bool>(settings->value(nx_ms_conf::ENABLE_MULTIPLE_INSTANCES).toInt());
    result.isRemovableDrivesAllowed = includeNonHdd
        ? true
        : static_cast<bool>(settings->value(nx_ms_conf::ALLOW_REMOVABLE_STORAGES).toInt());
    result.isNetworkDrivesAllowed = includeNonHdd;

    result.partitions = qnPlatform->monitor()->totalPartitionSpaceInfo();
    result.dataDirectory = getDataDirectory();
    result.mediaFolderName = QnAppInfo::mediaFolderName();
    result.serverUuid = serverGuid();

#if defined (Q_OS_WIN)
    result.isWindows = true;
#endif

    return result;
}

} // namespace media_paths
} // namespace fs
} // namespace mediaserver
} // namespace nx
