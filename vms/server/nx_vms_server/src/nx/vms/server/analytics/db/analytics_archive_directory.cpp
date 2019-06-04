#include "analytics_archive_directory.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <analytics/db/config.h>

#include "object_type_dao.h"
#include "serializers.h"

namespace nx::analytics::db {

static constexpr QRect kFullRegion(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight);

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
    uint32_t objectsGroupId,
    uint32_t objectType,
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

    return archive->saveToArchive(
        timestamp, region, objectsGroupId, objectType, allAttributesHash);
}

QnTimePeriodList AnalyticsArchiveDirectory::matchPeriods(
    const std::vector<QnUuid>& deviceIds,
    Filter filter)
{
    fixFilterRegion(&filter);

    // TODO: #ak If there are more than one device given we can apply map/reduce to speed things up.

    std::vector<QnTimePeriodList> timePeriods;
    for (const auto& deviceId: deviceIds)
        timePeriods.push_back(matchPeriods(deviceId, filter));

    return QnTimePeriodList::mergeTimePeriods(timePeriods, filter.limit, filter.sortOrder);
}

QnTimePeriodList AnalyticsArchiveDirectory::matchPeriods(
    const QnUuid& deviceId,
    const Filter& filter)
{
    auto it = m_deviceIdToArchive.find(deviceId);
    if (it == m_deviceIdToArchive.end())
        return {};

    return it->second->matchPeriod(filter);
}

AnalyticsArchiveDirectory::ObjectMatchResult AnalyticsArchiveDirectory::matchObjects(
    const std::vector<QnUuid>& deviceIds,
    Filter filter)
{
    if (deviceIds.empty())
        return {};

    fixFilterRegion(&filter);
    filter.limit = std::min(
        filter.limit > 0 ? filter.limit : kMaxObjectLookupResultSet,
        kMaxObjectLookupResultSet);

    std::vector<std::pair<int64_t /*timestamp*/, int64_t /*objectGroupId*/>> objectGroups;
    for (const auto deviceId: deviceIds)
    {
        auto deviceResult = matchObjects(deviceId, filter);

        std::transform(
            deviceResult.data.begin(), deviceResult.data.end(),
            std::back_inserter(objectGroups),
            [](const auto& item) { return std::make_pair(item.timestampMs, item.objectGroupId); });
    }

    return toObjectMatchResult(filter, std::move(objectGroups));
}

AnalyticsArchiveDirectory::ObjectMatchResult AnalyticsArchiveDirectory::toObjectMatchResult(
    const Filter& filter,
    std::vector<std::pair<int64_t /*timestamp*/, int64_t /*objectGroupId*/>> objectGroups)
{
    if (filter.sortOrder == Qt::AscendingOrder)
        std::sort(objectGroups.begin(), objectGroups.end(), std::less<>());
    else
        std::sort(objectGroups.begin(), objectGroups.end(), std::greater<>());

    if (objectGroups.size() > filter.limit)
        objectGroups.erase(objectGroups.begin() + filter.limit, objectGroups.end());

    ObjectMatchResult result;
    std::transform(
        objectGroups.begin(), objectGroups.end(),
        std::back_inserter(result.objectGroups),
        std::mem_fn(&std::pair<int64_t, int64_t>::second));

    result.maxAnalyzedTime = std::chrono::milliseconds(objectGroups.back().first);

    return result;
}

AnalyticsArchive::Filter AnalyticsArchiveDirectory::prepareArchiveFilter(
    const db::Filter& filter,
    const ObjectTypeDao& objectTypeDao)
{
    AnalyticsArchive::Filter archiveFilter;

    if (!filter.objectTypeId.empty())
    {
        std::transform(
            filter.objectTypeId.begin(), filter.objectTypeId.end(),
            std::back_inserter(archiveFilter.objectTypes),
            [&objectTypeDao](const auto& objectType)
            {
                return objectTypeDao.objectTypeIdFromName(objectType);
            });
    }

    if (filter.boundingBox)
        archiveFilter.region = kFullRegion.intersected(translateToSearchGrid(*filter.boundingBox));
    archiveFilter.startTime = filter.timePeriod.startTime();
    archiveFilter.endTime = filter.timePeriod.endTime();
    archiveFilter.sortOrder = filter.sortOrder;
    if (filter.maxObjectsToSelect > 0)
        archiveFilter.limit = filter.maxObjectsToSelect;

    return archiveFilter;
}

void AnalyticsArchiveDirectory::fixFilterRegion(Filter* filter)
{
    if (filter->region.isEmpty())
        filter->region = kFullRegion;
    else
        filter->region = filter->region.intersected(kFullRegion);
}

nx::vms::server::metadata::AnalyticsArchive::MatchObjectsResult AnalyticsArchiveDirectory::matchObjects(
    const QnUuid& deviceId,
    const Filter& filter)
{
    auto it = m_deviceIdToArchive.find(deviceId);
    if (it == m_deviceIdToArchive.end())
        return {};

    return it->second->matchObjects(filter);
}

} // namespace nx::analytics::db
