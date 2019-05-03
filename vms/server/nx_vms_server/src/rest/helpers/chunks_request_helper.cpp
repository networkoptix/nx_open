#include "chunks_request_helper.h"

#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/sync_queue.h>

#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <api/helpers/chunks_request_data.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>

#include "motion/motion_helper.h"
#include "recorder/storage_manager.h"
#include "media_server/media_server_module.h"
#include <nx/vms/server/metadata/analytics_helper.h>

static const auto kAllArchive = QList<QnServer::ChunksCatalog>()
    << QnServer::LowQualityCatalog << QnServer::HiQualityCatalog;

QnChunksRequestHelper::QnChunksRequestHelper(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QnTimePeriodList QnChunksRequestHelper::load(const QnChunksRequestData& request) const
{
    // TODO: #akulikov #backup storages: Alter this for two storage managers kinds.

    const auto archivePeriods = QnStorageManager::getRecordedPeriods(
        serverModule(), request, kAllArchive);

    switch (request.periodsType)
    {
        case Qn::MotionContent:
        {
            const auto motionPeriods = serverModule()->motionHelper()->matchImage(request);
            return QnTimePeriodList::intersection(motionPeriods, archivePeriods);
        }

        case Qn::AnalyticsContent:
        {
            nx::vms::server::metadata::AnalyticsHelper helper(serverModule()->metadataDatabaseDir());
            const auto analiticsPeriods = helper.matchImage(request);
            //const auto analiticsPeriods = loadAnalyticsTimePeriods(request);
            return QnTimePeriodList::intersection(analiticsPeriods, archivePeriods);
        }

        case Qn::RecordingContent:
        default:
            return archivePeriods;
    }
}

QnTimePeriodList QnChunksRequestHelper::loadAnalyticsTimePeriods(
    const QnChunksRequestData& request) const
{
    using namespace std::chrono;
    using namespace nx::analytics::storage;

    if (request.resList.empty())
        return QnTimePeriodList();

    nx::utils::SyncQueue<std::tuple<ResultCode, QnTimePeriodList>> resultsPerCamera;
    int lookupsOngoing = 0;

    Filter filter;
    if (request.analyticsStorageFilter)
        filter = *request.analyticsStorageFilter;
    filter.timePeriod.setStartTime(milliseconds(request.startTimeMs));
    filter.timePeriod.setDuration(milliseconds(request.endTimeMs - request.startTimeMs));
    filter.maxObjectsToSelect = request.limit;
    filter.sortOrder = request.sortOrder;

    TimePeriodsLookupOptions options;
    options.detailLevel = request.detailLevel;

    for (const auto& cameraResource: request.resList)
    {
        filter.deviceIds = std::vector<QnUuid>{cameraResource->getId()};

        ++lookupsOngoing;
        serverModule()->analyticsEventsStorage()->lookupTimePeriods(
            filter,
            options,
            [&resultsPerCamera](
                ResultCode resultCode,
                QnTimePeriodList timePeriods)
            {
                resultsPerCamera.push(std::make_tuple(resultCode, std::move(timePeriods)));
            });
    }

    QnTimePeriodList totalPeriodList;
    for (int i = 0; i < lookupsOngoing; ++i)
    {
        auto result = resultsPerCamera.pop();
        if (std::get<0>(result) != ResultCode::ok)
        {
            NX_DEBUG(QnLog::MAIN_LOG_ID,
                lm("Failed to fetch analytics time periods for camera TODO: %1")
                    .args(QnLexical::serialized(std::get<0>(result))));
            continue;
        }

        auto timePeriodList = std::move(std::get<1>(result));
        // NOTE: Assuming time periods are sorted by startTime ascending.
        if (totalPeriodList.empty())
            totalPeriodList.swap(timePeriodList);
        else
            QnTimePeriodList::unionTimePeriods(totalPeriodList, timePeriodList);
    }

    // We consider intervals with zero (i.e. unspecified) length as intervals with minimal length.
    for (auto& timePeriod: totalPeriodList)
    {
        if (timePeriod.isEmpty())
            timePeriod.setEndTime(timePeriod.startTime() + milliseconds(1));
    }

    return totalPeriodList;
}
