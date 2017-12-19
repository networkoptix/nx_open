#pragma once

#include <nx/mediaserver/fs/media_paths/media_paths_filter_config.h>

namespace nx {
namespace mediaserver {
namespace fs {
namespace media_paths {
namespace impl {

class Filter
{
public:
    explicit Filter(FilterConfig filterConfig);
    QStringList get() const;
private:
    FilterConfig m_filterConfig;

    QList<QnPlatformMonitor::PartitionSpace> filteredPartitions() const;
    void appendServerGuidPostFix(QStringList* paths) const;
    QString amendPath(const QString& path) const;
};

} // namespace impl
} // namespace media_paths
} // namespace fs
} // namespace mediaserver
} // namespace nx