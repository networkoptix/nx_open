#pragma once

#include <nx/vms/server/fs/media_paths/media_paths_filter_config.h>

namespace nx {
namespace vms::server {
namespace fs {
namespace media_paths {
namespace detail {

class Filter
{
public:
    explicit Filter(FilterConfig filterConfig);
    QStringList get() const;
private:
    FilterConfig m_filterConfig;

    QList<nx::vms::server::PlatformMonitor::PartitionSpace> filteredPartitions() const;
    void appendServerGuidPostFix(QStringList* paths) const;
    QString amendPath(const QString& path) const;
};

} // namespace detail
} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx