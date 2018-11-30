#pragma once

#include <nx/utils/uuid.h>
#include <platform/platform_abstraction.h>

namespace nx {
namespace vms::server {

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

    static FilterConfig createDefault(
        QnPlatformAbstraction* platform,
        bool includeNonHdd,
        const nx::vms::server::Settings* settings);
};

} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx
