#include "wearable_camera_rest_handler.h"

#include <QtCore/QFile>

#include <nx/utils/std/cpp14.h>
#include <utils/common/synctime.h>

#include <common/common_module.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/vms/common/p2p/downloader/downloader.h>

#include <api/model/wearable_camera_reply.h>
#include <api/model/wearable_status_reply.h>
#include <api/model/wearable_prepare_data.h>
#include <api/model/wearable_prepare_reply.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/user_access_data.h>

#include <recorder/storage_manager.h>
#include <media_server/media_server_module.h>
#include <media_server/wearable_upload_manager.h>
#include <media_server/wearable_lock_manager.h>
#include <rest/server/rest_connection_processor.h>

namespace {
const qint64 kDetalizationLevelMs = 1;
const qint64 kMinChunkCheckSize = 100 * 1024 * 1024;
const qint64 kOneDayMSecs = 1000ll * 3600ll * 24ll;

QnTimePeriod shrinkPeriod(const QnTimePeriod& local, const QnTimePeriodList& remote)
{
    QnTimePeriodList locals{ local };
    locals.excludeTimePeriods(remote);

    if (!locals.empty())
        return locals[0];
    return QnTimePeriod();
}

} // namespace

using namespace nx::vms::server::recorder;

QnWearableCameraRestHandler::QnWearableCameraRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QStringList QnWearableCameraRestHandler::cameraIdUrlParams() const
{
    return {lit("cameraId")};
}

int QnWearableCameraRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QString action = extractAction(path);
    if (action == "status")
        return executeStatus(params, result, owner);

    return nx::network::http::StatusCode::notFound;
}

int QnWearableCameraRestHandler::executePost(
    const QString& path,
    const QnRequestParams& params,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QString action = extractAction(path);
    if (action == "add")
        return executeAdd(params, result, owner);

    if (action == "prepare")
        return executePrepare(params, body, result, owner);

    if (action == "consume")
        return executeConsume(params, result, owner);

    if (action == "status")
        return executeStatus(params, result, owner);

    if (action == "lock")
        return executeLock(params, result, owner);

    if (action == "extend")
        return executeExtend(params, result, owner);

    if (action == "release")
        return executeRelease(params, result, owner);

    return nx::network::http::StatusCode::notFound;
}

int QnWearableCameraRestHandler::executeAdd(
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QString name;
    if (!requireParameter(params, lit("name"), result, &name))
        return nx::network::http::StatusCode::unprocessableEntity;

    nx::vms::api::CameraData apiCamera;
    apiCamera.physicalId = QnUuid::createUuid().toSimpleString();
    apiCamera.fillId();
    apiCamera.manuallyAdded = true;
    apiCamera.typeId = nx::vms::api::CameraData::kWearableCameraTypeId;
    apiCamera.parentId = owner->commonModule()->moduleGUID();
    apiCamera.name = name;
    // Note that physical id is in path, not in host.
    apiCamera.url = lit("wearable:///") + apiCamera.physicalId;

    ec2::ErrorCode code = owner->commonModule()->ec2Connection()
        ->getCameraManager(Qn::kSystemAccess)->addCameraSync(apiCamera);
    if (code != ec2::ErrorCode::ok)
    {
        result.setError(QnJsonRestResult::CantProcessRequest,
            lit("Error while adding camera '%1'.").arg(toString(code)));
        return nx::network::http::StatusCode::internalServerError;
    }

    nx::vms::api::CameraAttributesData apiAttributes;
    {
        QnCameraUserAttributePool::ScopedLock attributesLock(
            owner->commonModule()->cameraUserAttributesPool(), apiCamera.id);
        (*attributesLock)->audioEnabled = true;
        (*attributesLock)->motionType = Qn::MotionType::MT_NoMotion;
        ec2::fromResourceToApi(*attributesLock, apiAttributes);
    }

    owner->commonModule()->ec2Connection()
        ->getCameraManager(Qn::kSystemAccess)->saveUserAttributesSync({apiAttributes});
    // We don't care about return code here as wearable camera is already added.

    QnWearableCameraReply reply;
    reply.id = apiCamera.id;
    result.setReply(reply);

    return nx::network::http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executePrepare(const QnRequestParams& params,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnWearablePrepareData data;
    if(!QJson::deserialize(body, &data) || data.elements.empty())
        return nx::network::http::StatusCode::unprocessableEntity;

    QnVirtualCameraResourcePtr camera =
        owner->resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId);
    if (!camera)
        return nx::network::http::StatusCode::unprocessableEntity;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx::network::http::StatusCode::internalServerError;

    QnWearablePrepareReply reply;

    QnTimePeriod unionPeriod;
    for (const QnWearablePrepareDataElement& element : data.elements)
        unionPeriod.addPeriod(element.period);

    QnTimePeriodList serverTimePeriods = serverModule()->normalStorageManager()
        ->getFileCatalog(camera->getUniqueId(), QnServer::ChunksCatalog::HiQualityCatalog)
        ->getTimePeriods(
            unionPeriod.startTimeMs,
            unionPeriod.endTimeMs(),
            kDetalizationLevelMs,
            /*keepSmallChunks*/ false,
            /*limit*/ std::numeric_limits<int>::max(),
            Qt::SortOrder::AscendingOrder);

    for (const QnWearablePrepareDataElement& element: data.elements)
    {
        QnWearablePrepareReplyElement replyElement;
        replyElement.period = shrinkPeriod(element.period, serverTimePeriods);
        reply.elements.push_back(replyElement);
    }

    qint64 totalSize = 0;
    qint64 maxSize = 0;
    for (const QnWearablePrepareDataElement& element : data.elements)
    {
        totalSize += element.size;
        maxSize = std::max(maxSize, element.size);
    }

    QnWearableStorageStats stats = uploader->storageStats();

    if (maxSize > stats.downloaderBytesAvailable || totalSize > stats.totalBytesAvailable)
        reply.storageCleanupNeeded = true;
    if (maxSize > stats.downloaderBytesFree || !stats.haveStorages)
        reply.storageFull = true;

    result.setReply(reply);
    return nx::network::http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executeStatus(const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnWearableLockManager* locker = lockManager(result);
    if (!locker)
        return nx::network::http::StatusCode::internalServerError;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx::network::http::StatusCode::internalServerError;

    QnWearableLockInfo info = locker->lockInfo(cameraId);
    WearableArchiveSynchronizationState state = uploader->state(cameraId);

    QnWearableStatusReply reply;
    reply.success = true;
    reply.locked = info.locked;
    reply.consuming = state.isConsuming();
    reply.userId = info.userId;
    reply.progress = state.progress();
    result.setReply(reply);

    return nx::network::http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executeLock(const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnUuid userId;
    if (!requireParameter(params, lit("userId"), result, &userId))
        return nx::network::http::StatusCode::unprocessableEntity;

    qint64 ttl;
    if (!requireParameter(params, lit("ttl"), result, &ttl))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnWearableLockManager* locker = lockManager(result);
    if (!locker)
        return nx::network::http::StatusCode::internalServerError;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx::network::http::StatusCode::internalServerError;

    QnUuid token = QnUuid::createUuid();

    QnWearableStatusReply reply;
    reply.success =
        !uploader->isConsuming(cameraId)
        && locker->acquireLock(cameraId, token, userId, ttl);
    if (reply.success)
        reply.token = token;

    QnWearableLockInfo info = locker->lockInfo(cameraId);
    reply.locked = info.locked;
    reply.userId = info.userId;

    WearableArchiveSynchronizationState state = uploader->state(cameraId);
    reply.consuming = state.isConsuming();
    reply.progress = state.progress();

    result.setReply(reply);

    return nx::network::http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executeExtend(const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnUuid userId;
    if (!requireParameter(params, lit("userId"), result, &userId))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnUuid token;
    if (!requireParameter(params, lit("token"), result, &token))
        return nx::network::http::StatusCode::unprocessableEntity;
    if (token.isNull())
        return nx::network::http::StatusCode::unprocessableEntity;

    qint64 ttl;
    if (!requireParameter(params, lit("ttl"), result, &ttl))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnWearableLockManager* locker = lockManager(result);
    if (!locker)
        return nx::network::http::StatusCode::internalServerError;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx::network::http::StatusCode::internalServerError;

    bool success = locker->extendLock(cameraId, token, ttl);
    if (!success)
        success = locker->acquireLock(cameraId, token, userId, ttl);

    QnWearableLockInfo info = locker->lockInfo(cameraId);
    WearableArchiveSynchronizationState state = uploader->state(cameraId);

    QnWearableStatusReply reply;
    reply.success = success;
    reply.locked = info.locked;
    reply.consuming = state.isConsuming();
    reply.userId = info.userId;
    reply.progress = state.progress();
    result.setReply(reply);

    return nx::network::http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executeRelease(const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnUuid token;
    if (!requireParameter(params, lit("token"), result, &token))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnWearableLockManager* locker = lockManager(result);
    if (!locker)
        return nx::network::http::StatusCode::internalServerError;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx::network::http::StatusCode::internalServerError;

    QnWearableStatusReply reply;
    reply.success = locker->releaseLock(cameraId, token);

    QnWearableLockInfo info = locker->lockInfo(cameraId);
    reply.locked = info.locked;
    reply.userId = info.userId;

    WearableArchiveSynchronizationState state = uploader->state(cameraId);
    reply.consuming = state.isConsuming();
    reply.progress = state.progress();
    result.setReply(reply);

    return nx::network::http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executeConsume(
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx::network::http::StatusCode::unprocessableEntity;

    qint64 startTimeMs;
    if (!requireParameter(params, lit("startTime"), result, &startTimeMs))
        return nx::network::http::StatusCode::unprocessableEntity;

    QString uploadId;
    if (!requireParameter(params, lit("uploadId"), result, &uploadId))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnUuid token;
    if (!requireParameter(params, lit("token"), result, &token))
        return nx::network::http::StatusCode::unprocessableEntity;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx::network::http::StatusCode::internalServerError;

    QnWearableLockManager* locker = lockManager(result);
    if (!locker)
        return nx::network::http::StatusCode::internalServerError;

    if (!locker->extendLock(cameraId, token, 0))
        return nx::network::http::StatusCode::notAllowed;

    if (!uploader->consume(cameraId, token, uploadId, startTimeMs))
        return nx::network::http::StatusCode::unprocessableEntity;

    return nx::network::http::StatusCode::ok;
}

QnWearableLockManager* QnWearableCameraRestHandler::lockManager(QnJsonRestResult& result) const
{
    if (QnWearableLockManager* manager = serverModule()->findInstance<QnWearableLockManager>())
        return manager;

    result.setError(QnJsonRestResult::CantProcessRequest,
        lit("QnWearableLockManager is not initialized."));
    return nullptr;
}

QnWearableUploadManager* QnWearableCameraRestHandler::uploadManager(QnJsonRestResult& result) const
{
    if (QnWearableUploadManager* manager = serverModule()->findInstance<QnWearableUploadManager>())
        return manager;

    result.setError(QnJsonRestResult::CantProcessRequest,
        lit("QnWearableUploadManager is not initialized."));
    return nullptr;
}

