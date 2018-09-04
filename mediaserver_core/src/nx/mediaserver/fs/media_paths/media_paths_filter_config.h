#pragma once

#include <nx/utils/uuid.h>
#include <platform/monitoring/platform_monitor.h>

namespace nx {
namespace mediaserver {

class Settings;

namespace fs {
namespace media_paths {

struct FilterConfig
{
    FilterConfig(FilterConfig&&) = default;
    FilterConfig& operator=(FilterConfig&&) = default;
    FilterConfig() = default;

    QList<QnPlatformMonitor::PartitionSpace> partitions;
    bool isMultipleInstancesAllowed = false;
    bool isNetworkDrivesAllowed = false;
    bool isRemovableDrivesAllowed = false;
    QString dataDirectory;
    QString mediaFolderName;
    QnUuid serverUuid;
    bool isWindows = false;

    static FilterConfig createDefault(bool includeNonHdd, const nx::mediaserver::Settings* settings);
};

} // namespace media_paths
} // namespace fs
} // namespace mediaserver
} // namespace nx
