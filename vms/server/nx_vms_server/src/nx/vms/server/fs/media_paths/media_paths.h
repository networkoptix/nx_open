#pragma once

#include <QtCore/QStringList>

namespace nx {
namespace vms::server {
namespace fs {
namespace media_paths {

struct FilterConfig;
QStringList get(FilterConfig filterConfig);
bool isMounted(FilterConfig config, const QString& path);

} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx
