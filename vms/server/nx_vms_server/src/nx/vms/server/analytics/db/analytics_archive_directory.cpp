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
    uint32_t trackGroupId,
    uint32_t objectType,
    int64_t allAttributesHash)
{
    if (auto archive = openOrGetArchive(deviceId);
        archive != nullptr)
    {
        NX_VERBOSE(this, "Saving (%1; %2)", timestamp, trackGroupId);

        return archive->saveToArchive(
            timestamp, region, trackGroupId, objectType, allAttributesHash);
    }
    else
    {
        return false;
    }
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
    if (auto archive = openOrGetArchive(deviceId);
        archive != nullptr)
    {
        return archive->matchPeriod(filter);
    }
    else
    {
        return {};
    }
}

AnalyticsArchiveDirectory::ObjectTrackMatchResult AnalyticsArchiveDirectory::matchObjects(
    const std::vector<QnUuid>& deviceIds,
    Filter filter)
{
    if (deviceIds.empty())
        return {};

    fixFilterRegion(&filter);
    filter.limit = std::min(
        filter.limit > 0 ? filter.limit : kMaxObjectLookupResultSet,
        kMaxObjectLookupResultSet);

    std::vector<
        std::pair<std::chrono::milliseconds /*timestamp*/, int64_t /*trackGroupId*/>> trackGroups;

    for (const auto deviceId: deviceIds)
    {
        auto deviceResult = matchObjects(deviceId, filter);

        std::transform(
            deviceResult.data.begin(), deviceResult.data.end(),
            std::back_inserter(trackGroups),
            [this](const auto& item)
            {
                NX_VERBOSE(this, "Found (%1; %2)", item.timestampMs, item.trackGroupId);

                return std::make_pair(
                    std::chrono::milliseconds(item.timestampMs), item.trackGroupId);
            });
    }

    return toObjectTrackMatchResult(filter, std::move(trackGroups));
}

AnalyticsArchiveDirectory::ObjectTrackMatchResult
    AnalyticsArchiveDirectory::toObjectTrackMatchResult(
        const Filter& filter,
        TrackGroups trackGroups)
{
    if (filter.sortOrder == Qt::AscendingOrder)
        std::sort(trackGroups.begin(), trackGroups.end(), std::less<>());
    else
        std::sort(trackGroups.begin(), trackGroups.end(), std::greater<>());

    ObjectTrackMatchResult result;
    std::transform(
        trackGroups.begin(), trackGroups.end(),
        std::back_inserter(result.trackGroups),
        std::mem_fn(&std::pair<std::chrono::milliseconds, int64_t>::second));

    if (!trackGroups.empty())
    {
        result.timePeriod.setStartTime(
            std::min<>(trackGroups.front().first, trackGroups.back().first));

        result.timePeriod.setEndTime(
            std::max<>(trackGroups.front().first, trackGroups.back().first));
    }

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
    if (filter.maxObjectTracksToSelect > 0)
        archiveFilter.limit = filter.maxObjectTracksToSelect;

    return archiveFilter;
}

AnalyticsArchiveImpl* AnalyticsArchiveDirectory::openOrGetArchive(
    const QnUuid& deviceId)
{
    QnMutexLocker lock(&m_mutex);

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
                return nullptr;
            archive = std::make_unique<AnalyticsArchiveImpl>(m_dataDir, camera->getPhysicalId());
        }
        else
        {
            archive = std::make_unique<AnalyticsArchiveImpl>(m_dataDir, deviceId.toSimpleString());
        }
#endif
    }

    return archive.get();
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
    if (auto archive = openOrGetArchive(deviceId);
        archive != nullptr)
    {
        return archive->matchObjects(filter);
    }
    else
    {
        return {};
    }
}

} // namespace nx::analytics::db
