#pragma once

#include <QtCore/QStringList>

namespace nx {
namespace vms::server {
namespace fs {
namespace media_paths {

struct FilterConfig;
QStringList getMediaPaths(FilterConfig filterConfig);

} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx
