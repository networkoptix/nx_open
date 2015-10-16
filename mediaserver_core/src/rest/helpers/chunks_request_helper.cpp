#include "chunks_request_helper.h"

#include <api/helpers/chunks_request_data.h>

#include <core/resource/camera_resource.h>

#include <motion/motion_helper.h>

#include <recorder/storage_manager.h>

#include <utils/serialization/json.h>
#include <utils/serialization/json_functions.h>

QnTimePeriodList QnChunksRequestHelper::load(const QnChunksRequestData& request)
{
    QnTimePeriodList periods;
    switch (request.periodsType) {
    case Qn::MotionContent:
        {
            QList<QRegion> motionRegions = QJson::deserialized<QList<QRegion>>(request.filter.toUtf8());
            periods = QnMotionHelper::instance()->matchImage(motionRegions, request.resList, request.startTimeMs, request.endTimeMs, request.detailLevel);
        }
        break;
    case Qn::RecordingContent:
    default:
        periods = qnStorageMan->getRecordedPeriods(request.resList, request.startTimeMs, request.endTimeMs, request.detailLevel, request.keepSmallChunks,
            QList<QnServer::ChunksCatalog>() << QnServer::LowQualityCatalog << QnServer::HiQualityCatalog,
            request.limit);
        break;
    }

    return periods;
}
