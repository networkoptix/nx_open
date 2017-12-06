#include "chunks_request_helper.h"

#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/sync_queue.h>

#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <api/helpers/chunks_request_data.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/security_cam_resource.h>

#include "motion/motion_helper.h"
#include "recorder/storage_manager.h"
#include "media_server/media_server_module.h"

QnTimePeriodList QnChunksRequestHelper::load(const QnChunksRequestData& request)
{
    // TODO: #akulikov #backup storages: Alter this for two storage managers kinds.
    QnTimePeriodList periods;
    switch (request.periodsType)
    {
        case Qn::MotionContent:
        {
            QList<QRegion> motionRegions =
                QJson::deserialized<QList<QRegion>>(request.filter.toUtf8());
            periods = QnMotionHelper::instance()->matchImage(
                motionRegions, request.resList, request.startTimeMs, request.endTimeMs,
                request.detailLevel.count());
            break;
        }

        case Qn::AnalyticsContent:
            periods = loadAnalyticsTimePeriods(request);
            break;

        case Qn::RecordingContent:
        default:
            periods = QnStorageManager::getRecordedPeriods(
                request.resList, request.startTimeMs, request.endTimeMs, request.detailLevel.count(),
                request.keepSmallChunks,
                QList<QnServer::ChunksCatalog>()
                    << QnServer::LowQualityCatalog << QnServer::HiQualityCatalog,
                request.limit);
            break;
    }

    return periods;
}

QnTimePeriodList QnChunksRequestHelper::loadAnalyticsTimePeriods(
    const QnChunksRequestData& request)
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
    // TODO: #ak request.limit.

    TimePeriodsLookupOptions options;
    options.detailLevel = request.detailLevel;

    for (const auto& cameraResource: request.resList)
    {
        filter.deviceId = cameraResource->getId();

        ++lookupsOngoing;
        qnServerModule->analyticsEventsStorage()->lookupTimePeriods(
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

    return totalPeriodList;
}
