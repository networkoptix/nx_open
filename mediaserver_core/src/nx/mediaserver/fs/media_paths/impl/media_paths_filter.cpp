#include <QtCore>
#include <algorithm>
#include <utils/common/util.h>
#include "media_paths_filter.h"

namespace nx {
namespace mediaserver {
namespace fs {
namespace media_paths {
namespace impl {

namespace {

void filterOut(
    QList<QnPlatformMonitor::PartitionSpace>* list,
    QnPlatformMonitor::PartitionType partitionType)
{
    list->erase(
        std::remove_if(
            list->begin(), list->end(),
            [partitionType](const QnPlatformMonitor::PartitionSpace& partition)
    {
        return partition.type == partitionType;
    }), list->end());
}

class PathAmender
{
public:
    PathAmender(const QString& path, const FilterConfig& filterConfig):
        m_path(path),
        m_filterConfig(filterConfig)
    {
        if (m_filterConfig.isWindows)
        {
            amendPath();
            return;
        }

        amendLinuxPath();
    }

    QString take() { return std::move(m_path); }
private:
    QString m_path;
    const FilterConfig& m_filterConfig;

    void amendPath()
    {
        m_path = QDir::toNativeSeparators(m_path) + lit("/") + m_filterConfig.mediaFolderName;
        m_path = QDir::cleanPath(m_path);
    }

    void amendLinuxPath()
    {
        const QString dataDirPath = QDir::cleanPath(m_filterConfig.dataDirectory + "/data");
        if (isDataDirectoryOnThisDrive(m_path, dataDirPath))
        {
            m_path = dataDirPath;
            return;
        }

        amendPath();
    }

    static bool isDataDirectoryOnThisDrive(const QString& drivePath, const QString& dataDirPath)
    {
        return dataDirPath.startsWith(drivePath);
    }
};

} // namespace

Filter::Filter(FilterConfig filterConfig) :
    m_filterConfig(std::move(filterConfig))
{}

QStringList Filter::get() const
{
    QStringList result;
    for (const auto& partition : filteredPartitions())
        result.push_back(amendPath(partition.path));

    if (m_filterConfig.isMultipleInstancesAllowed)
        appendServerGuidPostFix(&result);

    return result;
}

QList<QnPlatformMonitor::PartitionSpace> Filter::filteredPartitions() const
{
    auto partitions = m_filterConfig.partitions;
    if (!m_filterConfig.isRemovableDrivesAllowed)
        filterOut(&partitions, QnPlatformMonitor::RemovableDiskPartition);

    if (!m_filterConfig.isNetworkDrivesAllowed)
        filterOut(&partitions, QnPlatformMonitor::RemovableDiskPartition);

    return partitions;
}

QString Filter::amendPath(const QString& path) const
{
    return PathAmender(path, m_filterConfig).take();
}

void Filter::appendServerGuidPostFix(QStringList* paths) const
{
    for (QString& path : *paths)
        path = closeDirPath(path) + m_filterConfig.serverUuid.toString();
}

} // namespace impl
} // namespace media_paths
} // namespace fs
} // namespace mediaserver
} // namespace nx