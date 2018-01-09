#include "media_paths.h"
#include "detail/media_paths_filter.h"

namespace nx {
namespace mediaserver {
namespace fs {
namespace media_paths {

struct FilterConfig;
QStringList get(FilterConfig filterConfig)
{
    return detail::Filter(std::move(filterConfig)).get();
}

} // namespace media_paths
} // namespace fs
} // namespace mediaserver
} // namespace nx