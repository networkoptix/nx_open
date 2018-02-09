#include "wearable_camera_rest_handler.h"

#include <QtCore/QFile>

#include <nx_ec/ec_api.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <api/model/wearable_camera_reply.h>
#include <api/model/wearable_status_reply.h>
#include <recorder/wearable_archive_synchronizer.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/user_access_data.h>
#include <common/common_module.h>
#include <plugins/resource/wearable/wearable_camera_resource.h>
#include <media_server/media_server_module.h>
#include <media_server/wearable_upload_manager.h>
#include <media_server/wearable_lock_manager.h>

#include <nx/utils/std/cpp14.h>
#include "core/resource/camera_user_attribute_pool.h"
#include "nx_ec/data/api_conversion_functions.h"

using namespace nx::mediaserver_core::recorder;

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

    return nx_http::StatusCode::notFound;
}

int QnWearableCameraRestHandler::executePost(
    const QString& path,
    const QnRequestParams& params,
    const QByteArray& /*body*/,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QString action = extractAction(path);
    if (action == "add")
        return executeAdd(params, result, owner);

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

    return nx_http::StatusCode::notFound;
}

int QnWearableCameraRestHandler::executeAdd(
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QString name;
    if (!requireParameter(params, lit("name"), result, &name))
        return nx_http::StatusCode::invalidParameter;

    ec2::ApiCameraData apiCamera;
    apiCamera.physicalId = QnUuid::createUuid().toSimpleString();
    apiCamera.fillId();
    apiCamera.manuallyAdded = true;
    apiCamera.typeId = QnResourceTypePool::kWearableCameraTypeUuid;
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
        return nx_http::StatusCode::internalServerError;
    }

    ec2::ApiCameraAttributesData apiAttributes;
    {
        QnCameraUserAttributePool::ScopedLock attributesLock(
            owner->commonModule()->cameraUserAttributesPool(), apiCamera.id);
        (*attributesLock)->audioEnabled = true;
        fromResourceToApi(*attributesLock, apiAttributes);
    }

    owner->commonModule()->ec2Connection()
        ->getCameraManager(Qn::kSystemAccess)->saveUserAttributesSync({apiAttributes});
    // We don't care about return code here as wearable camera is already added.

    QnWearableCameraReply reply;
    reply.id = apiCamera.id;
    result.setReply(reply);

    return nx_http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executeStatus(const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx_http::StatusCode::invalidParameter;

    QnWearableLockManager* locker = lockManager(result);
    if (!locker)
        return nx_http::StatusCode::internalServerError;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx_http::StatusCode::internalServerError;

    QnWearableLockInfo info = locker->lockInfo(cameraId);

    QnWearableStatusReply reply;
    reply.success = true;
    reply.locked = info.locked;
    reply.consuming = uploader->isConsuming(cameraId);
    reply.userId = info.userId;
    reply.progress = 0; // TODO: #wearable
    result.setReply(reply);

    return nx_http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executeLock(const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx_http::StatusCode::invalidParameter;

    QnUuid userId;
    if (!requireParameter(params, lit("userId"), result, &userId))
        return nx_http::StatusCode::invalidParameter;

    qint64 ttl;
    if (!requireParameter(params, lit("ttl"), result, &ttl))
        return nx_http::StatusCode::invalidParameter;

    QnWearableLockManager* locker = lockManager(result);
    if (!locker)
        return nx_http::StatusCode::internalServerError;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx_http::StatusCode::internalServerError;

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

    return nx_http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executeExtend(const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx_http::StatusCode::invalidParameter;

    QnUuid userId;
    if (!requireParameter(params, lit("userId"), result, &userId))
        return nx_http::StatusCode::invalidParameter;

    QnUuid token;
    if (!requireParameter(params, lit("token"), result, &token))
        return nx_http::StatusCode::invalidParameter;
    if (token.isNull())
        return nx_http::StatusCode::invalidParameter;

    qint64 ttl;
    if (!requireParameter(params, lit("ttl"), result, &ttl))
        return nx_http::StatusCode::invalidParameter;

    QnWearableLockManager* locker = lockManager(result);
    if (!locker)
        return nx_http::StatusCode::internalServerError;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx_http::StatusCode::internalServerError;

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

    return nx_http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executeRelease(const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx_http::StatusCode::invalidParameter;

    QnUuid token;
    if (!requireParameter(params, lit("token"), result, &token))
        return nx_http::StatusCode::invalidParameter;

    QnWearableLockManager* locker = lockManager(result);
    if (!locker)
        return nx_http::StatusCode::internalServerError;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx_http::StatusCode::internalServerError;

    QnWearableStatusReply reply;
    reply.success = locker->releaseLock(cameraId, token);

    QnWearableLockInfo info = locker->lockInfo(cameraId);
    reply.locked = info.locked;
    reply.userId = info.userId;

    WearableArchiveSynchronizationState state = uploader->state(cameraId);
    reply.consuming = state.isConsuming();
    reply.progress = state.progress();
    result.setReply(reply);

    return nx_http::StatusCode::ok;
}

int QnWearableCameraRestHandler::executeConsume(
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return nx_http::StatusCode::invalidParameter;

    qint64 startTimeMs;
    if (!requireParameter(params, lit("startTime"), result, &startTimeMs))
        return nx_http::StatusCode::invalidParameter;

    QString uploadId;
    if (!requireParameter(params, lit("uploadId"), result, &uploadId))
        return nx_http::StatusCode::invalidParameter;

    QnUuid token;
    if (!requireParameter(params, lit("token"), result, &token))
        return nx_http::StatusCode::invalidParameter;

    QnWearableUploadManager* uploader = uploadManager(result);
    if (!uploader)
        return nx_http::StatusCode::internalServerError;

    QnWearableLockManager* locker = lockManager(result);
    if (!locker)
        return nx_http::StatusCode::internalServerError;

    if (!locker->extendLock(cameraId, token, 0))
        return nx_http::StatusCode::notAllowed;

    if (!uploader->consume(cameraId, token, uploadId, startTimeMs))
        return nx_http::StatusCode::invalidParameter;

    return nx_http::StatusCode::ok;
}

QnWearableLockManager* QnWearableCameraRestHandler::lockManager(QnJsonRestResult& result) const
{
    if (QnWearableLockManager* manager = qnServerModule->findInstance<QnWearableLockManager>())
        return manager;

    result.setError(QnJsonRestResult::CantProcessRequest,
        lit("QnWearableLockManager is not initialized."));
    return nullptr;
}

QnWearableUploadManager* QnWearableCameraRestHandler::uploadManager(QnJsonRestResult& result) const
{
    if (QnWearableUploadManager* manager = qnServerModule->findInstance<QnWearableUploadManager>())
        return manager;

    result.setError(QnJsonRestResult::CantProcessRequest,
        lit("QnWearableUploadManager is not initialized."));
    return nullptr;
}

