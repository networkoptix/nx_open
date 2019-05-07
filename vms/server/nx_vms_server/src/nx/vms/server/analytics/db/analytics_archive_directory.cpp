#include "analytics_archive_directory.h"

namespace nx::analytics::db {

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

QnTimePeriodList AnalyticsArchiveDirectory::matchPeriods(
    const std::vector<QnUuid>& deviceIds,
    const Filter& filter)
{
    // TODO: #ak If there are more than one device given we can apply map/reduce to speed things up.
    
    QnTimePeriodList allTimePeriods;
    for (const auto& deviceId: deviceIds)
    {
        auto timePeriods = matchPeriods(deviceId, filter);
        allTimePeriods = QnTimePeriodList::mergeTimePeriods({allTimePeriods, timePeriods});
    }

    return allTimePeriods;
}

QnTimePeriodList AnalyticsArchiveDirectory::matchPeriods(
    const QnUuid& deviceId,
    const Filter& filter)
{
    if (auto it = m_deviceIdToArchive.find(deviceId); it != m_deviceIdToArchive.end())
        return it->second->matchPeriod(filter);
    return {};
}

} // namespace nx::analytics::db
