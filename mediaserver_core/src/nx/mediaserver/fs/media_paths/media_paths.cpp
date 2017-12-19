#include "media_paths.h"
#include "impl/media_paths_filter.h"

namespace nx {
namespace mediaserver {
namespace fs {
namespace media_paths {

struct FilterConfig;
QStringList mediaPaths(FilterConfig filterConfig)
{
    return impl::Filter(std::move(filterConfig)).get();
}

} // namespace media_paths
} // namespace fs
} // namespace mediaserver
} // namespace nx