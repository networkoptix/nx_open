#include "media_paths.h"
#include "detail/media_paths_filter.h"

#include <QtCore/QDir>

namespace nx {
namespace vms::server {
namespace fs {
namespace media_paths {

QStringList get(FilterConfig filterConfig)
{
    return detail::Filter(std::move(filterConfig)).get();
}

bool isMounted(
    FilterConfig config,
    const QString& path,
    nx::utils::MoveOnlyFunc<QString(const QString&)> toCanonical)
{
    if (path.contains(config.dataDirectory))
        return true;

    int mediaFolderPos = path.indexOf(config.mediaFolderName);
    QString toCheck = QDir::cleanPath(mediaFolderPos == -1 ? path : path.left(mediaFolderPos));
    toCheck = toCanonical(toCheck);

    return std::any_of(
        config.partitions.cbegin(), config.partitions.cend(),
        [&toCheck](const auto& partition) { return partition.path == toCheck; });
}

} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx
