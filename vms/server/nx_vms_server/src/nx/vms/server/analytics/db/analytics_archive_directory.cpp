#include "analytics_archive_directory.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::analytics::db {

AnalyticsArchiveDirectory::AnalyticsArchiveDirectory(
    QnMediaServerModule* mediaServerModule,
    const QString& dataDir)
    :
    m_mediaServerModule(mediaServerModule),
    m_dataDir(dataDir)
{
}

bool AnalyticsArchiveDirectory::saveToArchive(
    const QnUuid& deviceId,
    std::chrono::milliseconds timestamp,
    const std::vector<QRect>& region,
    int64_t objectType,
    int64_t allAttributesHash)
{
    auto& archive = m_deviceIdToArchive[deviceId];
    if (!archive)
    {
#ifdef USE_IN_MEMORY_ARCHIVE
        archive = std::make_unique<AnalyticsArchiveImpl>(m_dataDir, deviceId);
#else
        if (m_mediaServerModule)
        {
            auto camera = m_mediaServerModule->resourcePool()
                ->getResourceById<QnVirtualCameraResource>(deviceId);
            if (!camera)
                return false;
            archive = std::make_unique<AnalyticsArchiveImpl>(m_dataDir, camera->getPhysicalId());
        }
        else
        {
            archive = std::make_unique<AnalyticsArchiveImpl>(m_dataDir, deviceId.toSimpleString());
        }
#endif
    }

    return archive->saveToArchive(timestamp, region, objectType, allAttributesHash);
}

QnTimePeriodList AnalyticsArchiveDirectory::matchPeriods(
    const std::vector<QnUuid>& deviceIds,
    const Filter& filter)
{
    // TODO: #ak If there are more than one device given we can apply map/reduce to speed things up.

    std::vector<QnTimePeriodList> timePeriods;
    if (!deviceIds.empty())
    {
        for (const auto& deviceId: deviceIds)
            timePeriods.push_back(matchPeriods(deviceId, filter));
    }
    else
    {
        for (const auto& [deviceId, archive]: m_deviceIdToArchive)
            timePeriods.push_back(matchPeriods(deviceId, filter));
    }

    return QnTimePeriodList::mergeTimePeriods(timePeriods, filter.limit, filter.sortOrder);
}

QnTimePeriodList AnalyticsArchiveDirectory::matchPeriods(
    const QnUuid& deviceId,
    const Filter& filter)
{
    static constexpr QRect kFullRegion(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight);

    auto it = m_deviceIdToArchive.find(deviceId);
    if (it == m_deviceIdToArchive.end())
        return {};

#ifdef USE_IN_MEMORY_ARCHIVE
    return it->second->matchPeriod(filter);
#else
    AnalyticsArchiveImpl::AnalyticsFilter archiveFilter;
    archiveFilter.region = filter.region;
    if (archiveFilter.region.isEmpty())
    {
        archiveFilter.region = kFullRegion;
    }
    else
    {
        // Cutting everything beyond grid.
        archiveFilter.region = archiveFilter.region.intersected(kFullRegion);
    }
    archiveFilter.startTime = filter.timePeriod.startTime();
    archiveFilter.endTime = filter.timePeriod.endTime();
    archiveFilter.detailLevel = filter.detailLevel;
    archiveFilter.limit = filter.limit;
    archiveFilter.sortOrder = filter.sortOrder;
    std::copy(
        filter.objectTypes.begin(), filter.objectTypes.end(),
        std::back_inserter(archiveFilter.objectTypes));
    std::copy(
        filter.allAttributesHash.begin(), filter.allAttributesHash.end(),
        std::back_inserter(archiveFilter.allAttributesHash));

    return it->second->matchPeriod(archiveFilter);
#endif
}

} // namespace nx::analytics::db
