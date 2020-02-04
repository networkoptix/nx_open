#pragma once

#include <platform/monitoring/platform_monitor.h>
#include <nx/vms/server/fs/media_paths/media_paths_filter_config.h>

namespace nx {
namespace vms::server {
namespace fs {
namespace media_paths {

/**
 * PlatformMonitor::ParitionSpace alias. This alias is introduced to emphasize the key difference
 * from the latter - media_paths::Partition.url is a ready-to-use storage path.
 */
using Partition = nx::vms::server::PlatformMonitor::PartitionSpace;

namespace detail {

class Filter
{
public:
    explicit Filter(const FilterConfig& filterConfig);
    QList<Partition> get() const;

private:
    FilterConfig m_filterConfig;

    QList<nx::vms::server::PlatformMonitor::PartitionSpace> filteredPartitions() const;
    void appendServerGuidPostFix(QList<Partition>* partitions) const;
    void amendPath(QString& path) const;
};

} // namespace detail
} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx