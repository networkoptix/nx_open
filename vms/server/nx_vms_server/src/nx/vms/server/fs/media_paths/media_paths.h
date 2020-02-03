#pragma once

#include <QtCore/QList>

#include <nx/vms/server/fs/media_paths/detail/media_paths_filter.h>

namespace nx {
namespace vms::server {
namespace fs {
namespace media_paths {

struct FilterConfig;
QList<Partition> getMediaPartitions(const FilterConfig& filterConfig);

} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx
