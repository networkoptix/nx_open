#include "chunks_request_helper.h"

#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/sync_queue.h>

#include <analytics/db/abstract_storage.h>
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
            //nx::vms::server::metadata::AnalyticsHelper helper(serverModule()->metadataDatabaseDir());
            //const auto analiticsPeriods = helper.matchImage(request);
            const auto analiticsPeriods = loadAnalyticsTimePeriods(request);
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
    using namespace nx::analytics::db;

    if (request.resList.empty())
        return QnTimePeriodList();

    Filter filter;
    if (request.analyticsStorageFilter)
        filter = *request.analyticsStorageFilter;
    filter.timePeriod.setStartTime(milliseconds(request.startTimeMs));
    filter.timePeriod.setDuration(milliseconds(request.endTimeMs - request.startTimeMs));
    filter.maxObjectsToSelect = request.limit;
    filter.sortOrder = request.sortOrder;

    filter.deviceIds.clear();
    for (const auto& cameraResource: request.resList)
        filter.deviceIds.push_back(cameraResource->getId());

    TimePeriodsLookupOptions options;
    options.detailLevel = request.detailLevel;

    if (!request.filter.isEmpty())
    {
        const auto regions = QJson::deserialized<QList<QRegion>>(request.filter.toUtf8());
        if (!regions.isEmpty())
            options.region = regions.front();
        // TODO: #ak Not sure why using only first region. Done as in AnalyticsHelper::matchImage.
    }

    std::promise<std::tuple<ResultCode, QnTimePeriodList>> done;
    serverModule()->analyticsEventsStorage()->lookupTimePeriods(
        filter,
        options,
        [&done](ResultCode resultCode, QnTimePeriodList timePeriods)
        {
            done.set_value(std::make_tuple(resultCode, std::move(timePeriods)));
        });

    auto [resultCode, timePeriods] = done.get_future().get();
    if (resultCode != ResultCode::ok)
    {
        NX_DEBUG(this, "Failed to fetch analytics time periods for camera TODO: %1",
            QnLexical::serialized(resultCode));
    }

    return timePeriods;
}
