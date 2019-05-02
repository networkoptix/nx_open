#include "analytics_archive_directory.h"

namespace nx::analytics::storage {

AnalyticsArchiveDirectory::AnalyticsArchiveDirectory(const QString& dataDir):
    m_dataDir(dataDir)
{
}

bool AnalyticsArchiveDirectory::saveToArchive(
    const QnUuid& deviceId,
    std::chrono::milliseconds timestamp,
    const std::vector<QRectF>& region,
    int objectType,
    long long allAttributesHash)
{
    auto& archive = m_deviceIdToArchive[deviceId];
    if (!archive)
        archive = std::make_unique<AnalyticsArchive>(m_dataDir, deviceId);

    return archive->saveToArchive(timestamp, region, objectType, allAttributesHash);
}

QnTimePeriodList AnalyticsArchiveDirectory::matchPeriod(
    const QnUuid& deviceId,
    const Filter& filter)
{
    if (auto it = m_deviceIdToArchive.find(deviceId); it != m_deviceIdToArchive.end())
        return it->second->matchPeriod(filter);
    return {};
}

} // namespace nx::analytics::storage
