#include "analytics_archive_directory.h"

#include <QtCore/QDir>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <analytics/db/config.h>
#include <analytics/db/analytics_db_utils.h>

#include "attributes_dao.h"
#include "object_type_dao.h"

namespace nx::analytics::db {

static constexpr QRect kFullRegion(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight);

AnalyticsArchiveDirectory::AnalyticsArchiveDirectory(
    QnCommonModule* commonModule,
    const QString& dataDir)
    :
    m_commonModule(commonModule),
    m_dataDir(dataDir)
{
    if (!m_commonModule)
    {
        QDir dir(m_dataDir + "/metadata");
        const auto cameraDirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        for (const auto& cameraDir: cameraDirs)
        {
            const auto cameraId = QnUuid::fromStringSafe(cameraDir);
            if (cameraId.isNull())
                continue;

            openOrGetArchive(cameraId);
        }
    }
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
    std::vector<QnUuid> deviceIds,
    ArchiveFilter filter)
{
    if (deviceIds.empty())
        copyAllDeviceIds(&deviceIds);

    fixFilter(&filter);


    // TODO: #ak If there are more than one device given we can apply map/reduce to speed things up.

    std::vector<QnTimePeriodList> timePeriods;
    for (const auto& deviceId: deviceIds)
        timePeriods.push_back(matchPeriods(deviceId, filter));

    return QnTimePeriodList::mergeTimePeriods(timePeriods, filter.limit, filter.sortOrder);
}

QnTimePeriodList AnalyticsArchiveDirectory::matchPeriods(
    const QnUuid& deviceId,
    const ArchiveFilter& filter)
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
    std::vector<QnUuid> deviceIds,
    ArchiveFilter filter,
    const std::map<QnUuid, QnTimePeriod>* previousPeriods)
{
    using namespace std::chrono;

    if (deviceIds.empty())
        copyAllDeviceIds(&deviceIds);

    fixFilter(&filter);
    filter.limit = std::min(
        filter.limit > 0 ? filter.limit : kMaxObjectLookupResultSet,
        kMaxObjectLookupResultSet);

    DeviceResults deviceResults;
    for (const auto& deviceId: deviceIds)
    {
        ArchiveFilter filterCopy = filter;
        if (previousPeriods)
        {
            auto itr = previousPeriods->find(deviceId);
            if (itr != previousPeriods->end() && !itr->second.isEmpty())
            {
                if (filter.sortOrder == Qt::AscendingOrder)
                    filterCopy.startTime = milliseconds(itr->second.endTimeMs()) + 1ms;
                else
                    filterCopy.endTime = milliseconds(itr->second.startTime());
            }
        }

        deviceResults.emplace(deviceId, matchObjects(deviceId, filterCopy));
    }

    return toObjectTrackMatchResult(filter, std::move(deviceResults));
}

AnalyticsArchiveDirectory::ObjectTrackMatchResult
    AnalyticsArchiveDirectory::toObjectTrackMatchResult(
        const ArchiveFilter& filter,
        DeviceResults deviceResults)
{
    ObjectTrackMatchResult result;

    for (const auto& [deviceId, deviceResult]: deviceResults)
    {
        std::transform(
            deviceResult.data.begin(), deviceResult.data.end(),
            std::back_inserter(result.trackGroups),
            [this](const auto& item)
            {
                NX_VERBOSE(this, "Found (%1; %2)", item.timestampMs, item.trackGroupId);
                return item.trackGroupId;
            });
        result.timePeriods.emplace(deviceId, deviceResult.boundingPeriod);
    }

    if (filter.sortOrder == Qt::AscendingOrder)
        std::sort(result.trackGroups.begin(), result.trackGroups.end(), std::less<>());
    else
        std::sort(result.trackGroups.begin(), result.trackGroups.end(), std::greater<>());

    return result;
}

std::optional<ArchiveFilter> AnalyticsArchiveDirectory::prepareArchiveFilter(
    nx::sql::QueryContext* queryContext,
    const db::Filter& filter,
    const ObjectTypeDao& objectTypeDao,
    AttributesDao* attributesDao)
{
    ArchiveFilter archiveFilter;

    if (!filter.objectTypeId.empty())
    {
        for (const auto& name: filter.objectTypeId)
        {
            const auto id = objectTypeDao.objectTypeIdFromName(name);
            if (id != -1)
                archiveFilter.objectTypes.push_back(id);
        }

        if (archiveFilter.objectTypes.empty())
        {
            NX_DEBUG(typeid(AnalyticsArchiveDirectory), "No valid object type was specified");
            return std::nullopt;
        }
    }

    if (filter.boundingBox)
        archiveFilter.region = kFullRegion.intersected(translateToSearchGrid(*filter.boundingBox));
    archiveFilter.startTime = filter.timePeriod.startTime();
    archiveFilter.endTime = filter.timePeriod.endTime();
    archiveFilter.sortOrder = filter.sortOrder;
    if (filter.maxObjectTracksToSelect > 0)
        archiveFilter.limit = filter.maxObjectTracksToSelect;

    if (!filter.freeText.isEmpty())
    {
        nx::utils::ElapsedTimer timer;
        timer.restart();

        const auto attributeGroups =
            attributesDao->lookupCombinedAttributes(queryContext, filter.freeText);
        if (attributeGroups.empty())
        {
            NX_DEBUG(typeid(AnalyticsArchiveDirectory),
                "%1 text did not match anything", filter.freeText);
            return std::nullopt;
        }

        NX_DEBUG(typeid(AnalyticsArchiveDirectory),
            "Text '%1' lookup completed in %2", filter.freeText, timer.elapsed());

        std::copy(
            attributeGroups.begin(), attributeGroups.end(),
            std::back_inserter(archiveFilter.allAttributesHash));
    }

    return archiveFilter;
}

AnalyticsArchiveImpl* AnalyticsArchiveDirectory::openOrGetArchive(
    const QnUuid& deviceId)
{
    QnMutexLocker lock(&m_mutex);

    auto& archive = m_deviceIdToArchive[deviceId];
    if (!archive)
    {
        if (m_commonModule)
        {
            auto camera = m_commonModule->resourcePool()
                ->getResourceById<QnVirtualCameraResource>(deviceId);
            if (!camera)
                return nullptr;
            archive = std::make_unique<AnalyticsArchiveImpl>(m_dataDir, camera->getPhysicalId());
        }
        else
        {
            archive = std::make_unique<AnalyticsArchiveImpl>(m_dataDir, deviceId.toSimpleString());
        }
    }

    return archive.get();
}

void AnalyticsArchiveDirectory::fixFilter(ArchiveFilter* filter)
{
    std::sort(filter->allAttributesHash.begin(), filter->allAttributesHash.end());
    std::sort(filter->objectTypes.begin(), filter->objectTypes.end());

    if (filter->region.isEmpty())
        filter->region = kFullRegion;
    else
        filter->region = filter->region.intersected(kFullRegion);
}

AnalyticsArchive::MatchObjectsResult AnalyticsArchiveDirectory::matchObjects(
    const QnUuid& deviceId,
    const ArchiveFilter& filter)
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

void AnalyticsArchiveDirectory::copyAllDeviceIds(std::vector<QnUuid>* deviceIds)
{
    if (m_commonModule)
    {
        const auto cameraResources = m_commonModule->resourcePool()->getAllCameras(
            m_commonModule->resourcePool()->getResourceById(m_commonModule->moduleGUID()));

        for (const auto& camera: cameraResources)
            openOrGetArchive(camera->getId());
    }

    QnMutexLocker lock(&m_mutex);

    std::transform(
        m_deviceIdToArchive.begin(), m_deviceIdToArchive.end(),
        std::back_inserter(*deviceIds),
        [](const auto& item) { return item.first; });
}

} // namespace nx::analytics::db
