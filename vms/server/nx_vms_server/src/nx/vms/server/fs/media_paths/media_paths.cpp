#include "media_paths.h"

namespace nx {
namespace vms::server {
namespace fs {
namespace media_paths {

struct FilterConfig;
QList<Partition> getMediaPartitions(const FilterConfig& filterConfig)
{
    return detail::Filter(filterConfig).get();
}

} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx
