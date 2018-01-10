#include "wearable_camera_rest_handler.h"

#include <QtCore/QBuffer>
#include <QtCore/QFile> // TODO: drop

#include <nx_ec/ec_api.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <api/model/wearable_camera_reply.h>
#include <network/tcp_connection_priv.h>
#include <recorder/wearable_archive_synchronization_task.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/user_access_data.h>
#include <common/common_module.h>
#include <media_server/media_server_module.h>

QStringList QnWearableCameraRestHandler::cameraIdUrlParams() const
{
    return { lit("cameraId") };
}

int QnWearableCameraRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner) 
{
    QString action = extractAction(path);
    if (action == "add")
        return executeAdd(params, result, owner);
    else if (action == "process")
        return executeProcess(params, result, owner);
    else
        return CODE_NOT_FOUND;
}


int QnWearableCameraRestHandler::executeAdd(const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    QString name;
    if (!requireParameter(params, lit("name"), result, &name))
        return CODE_INVALID_PARAMETER;

    QnMediaServerResourcePtr server = owner->resourcePool()->getResourceById<QnMediaServerResource>(owner->commonModule()->moduleGUID());
    if (!server) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("No server in resource pool!"));
        return CODE_INTERNAL_ERROR;
    }

    ec2::ApiCameraData camera;
    camera.mac = QnLatin1Array();
    camera.physicalId = QnUuid::createUuid().toSimpleString();
    camera.manuallyAdded = true;
    camera.model = QString();
    camera.groupId = QString();
    camera.groupName = QString();
    camera.statusFlags = Qn::CSF_NoFlags;
    camera.vendor = QString();

    camera.fillId();
    camera.typeId = QnResourceTypePool::kWearableCameraTypeUuid;
    camera.parentId = server->getId();
    camera.name = name;
    camera.url = lit("wearable:///") + camera.physicalId; /* Note that physical id is in path, not in host. */

    ec2::ErrorCode code = owner->commonModule()->ec2Connection()->getCameraManager(Qn::kSystemAccess)->addCameraSync(camera);
    if (code != ec2::ErrorCode::ok) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("EC returned error '%1'.").arg(toString(code)));
        return CODE_INTERNAL_ERROR;
    }

    QnWearableCameraReply reply;
    reply.id = camera.id;
    result.setReply(reply);

    return CODE_OK;
}

int QnWearableCameraRestHandler::executeProcess(const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner) {
    QnUuid cameraId;
    if (!requireParameter(params, lit("cameraId"), result, &cameraId))
        return CODE_INVALID_PARAMETER;

    qint64 startTimeMs;
    if (!requireParameter(params, lit("startTime"), result, &startTimeMs))
        return CODE_INVALID_PARAMETER;

    QString uploadId;
    if (!requireParameter(params, lit("uploadId"), result, &uploadId))
        return CODE_INVALID_PARAMETER;

    QnSecurityCamResourcePtr camera = owner->resourcePool()->getResourceById(cameraId).dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return CODE_INVALID_PARAMETER;
    if (camera->getTypeId() != QnResourceTypePool::kWearableCameraTypeUuid)
        return CODE_INVALID_PARAMETER;

    using namespace nx::vms::common::p2p::downloader;
    Downloader* downloader = qnServerModule->findInstance<Downloader>();
    if (!downloader) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("DistributedFileDownloader is not initialized."));
        return CODE_INTERNAL_ERROR;
    }

    std::unique_ptr<QFile> file(new QFile(downloader->filePath(uploadId)));
    if (!file->open(QIODevice::ReadOnly)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Couldn't open file \"%1\" for reading.").arg(uploadId));
        return CODE_INTERNAL_ERROR;
    }

    nx::mediaserver_core::recorder::WearableArchiveSynchronizationTask task(owner->commonModule(), camera, file.release(), startTimeMs);
    task.execute();
    downloader->deleteFile(uploadId);

    return CODE_OK;
}
