#include <QtCore>
#include <algorithm>
#include <utils/common/util.h>
#include "media_paths_filter.h"

namespace nx {
namespace vms::server {
namespace fs {
namespace media_paths {
namespace detail {

namespace {

void filterOut(
    QList<nx::vms::server::PlatformMonitor::PartitionSpace>* list,
    nx::vms::server::PlatformMonitor::PartitionType partitionType)
{
    const auto newEndIt = std::remove_if(
        list->begin(), list->end(),
        [partitionType](const PlatformMonitor::PartitionSpace& partition)
        { return partition.type == partitionType; });

    using namespace nx::utils::log;
    if (nx::utils::log::isToBeLogged(Level::verbose))
    {
        for (auto it = newEndIt; it != list->cend(); ++it)
            NX_VERBOSE(typeid(Filter), "Filtering out partition: '%1' of type '%2'", it->path, it->type);
    }

    list->erase(newEndIt, list->end());
}

class PathAmender
{
public:
    PathAmender(QString& path, const FilterConfig& filterConfig):
        m_path(path),
        m_filterConfig(filterConfig)
    {
    }

    void operator()()
    {
        if (m_filterConfig.isWindows)
        {
            amendPath();
            return;
        }

        amendLinuxPath();
    }

private:
    QString& m_path;
    const FilterConfig& m_filterConfig;

    void amendPath()
    {
        m_path = m_path + QDir::separator() +  m_filterConfig.mediaFolderName;
        m_path = fixSeparators(QDir::cleanPath(m_path));
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

    QString fixSeparators(const QString& path)
    {
        QChar fromSeparator = '\\';
        QChar toSeparator = '/';
        if (m_filterConfig.isWindows)
        {
            fromSeparator = '/';
            toSeparator = '\\';
        }

        return removeDuplicates(QString(path).replace(fromSeparator, toSeparator), toSeparator);
    }

    QString removeDuplicates(const QString& source, QChar charToRemove)
    {
        QString result;
        for (int i = 0; i < source.size(); ++i)
        {
            result.append(source[i]);
            if (source[i] == charToRemove && !(m_filterConfig.isWindows && i == 0))
            {
                int j = i + 1;
                while (j < source.size() && source[j] == charToRemove)
                    ++j;
                i = j - 1;
            }
        }
        return result;
    }
};

} // namespace

Filter::Filter(const FilterConfig& filterConfig):
    m_filterConfig(std::move(filterConfig))
{}

QList<Partition> Filter::get() const
{
    QList<Partition> result;
    NX_VERBOSE(this, "Candidates: %1", containerString(m_filterConfig.partitions));
    for (auto& partition: filteredPartitions())
    {
        amendPath(partition.path);
        result.push_back(partition);
    }

    if (m_filterConfig.isMultipleInstancesAllowed)
        appendServerGuidPostFix(&result);

    std::sort(
        result.begin(), result.end(),
        [](const auto& p1, const auto& p2) { return p1.path < p2.path; });

    result.erase(
        std::unique(
            result.begin(), result.end(), [](const auto& p1, const auto& p2)
            {
                return p1.path == p2.path;
            }),
        result.end());

    return result;
}

QList<nx::vms::server::PlatformMonitor::PartitionSpace> Filter::filteredPartitions() const
{
    auto partitions = m_filterConfig.partitions;
    if (!m_filterConfig.isRemovableDrivesAllowed)
        filterOut(&partitions, nx::vms::server::PlatformMonitor::RemovableDiskPartition);

    if (!m_filterConfig.isNetworkDrivesAllowed)
        filterOut(&partitions, nx::vms::server::PlatformMonitor::NetworkPartition);

    return partitions;
}

void Filter::amendPath(QString& path) const
{
    PathAmender(path, m_filterConfig)();
}

void Filter::appendServerGuidPostFix(QList<Partition>* partitions) const
{
    for (auto& p : *partitions)
        p.path = closeDirPath(p.path) + m_filterConfig.serverUuid.toString();
}

} // namespace detail
} // namespace media_paths
} // namespace fs
} // namespace vms::server
} // namespace nx
