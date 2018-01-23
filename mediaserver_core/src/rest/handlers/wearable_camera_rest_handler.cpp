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
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/user_access_data.h>
#include <common/common_module.h>
#include <plugins/resource/wearable/wearable_camera_resource.h>
#include <media_server/media_server_module.h>

#include <nx/utils/std/cpp14.h>

QStringList QnWearableCameraRestHandler::cameraIdUrlParams() const
{
    return {lit("cameraId")};
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

    ec2::ApiCameraData camera;
    camera.physicalId = QnUuid::createUuid().toSimpleString();
    camera.fillId();
    camera.manuallyAdded = true;
    camera.typeId = QnResourceTypePool::kWearableCameraTypeUuid;
    camera.parentId = owner->commonModule()->moduleGUID();
    camera.name = name;
    // Note that physical id is in path, not in host.
    camera.url = lit("wearable:///") + camera.physicalId;

    const ec2::ErrorCode code = owner->commonModule()->ec2Connection()
        ->getCameraManager(Qn::kSystemAccess)->addCameraSync(camera);

    if (code != ec2::ErrorCode::ok)
    {
        result.setError(QnJsonRestResult::CantProcessRequest,
            lit("Error while adding camera '%1'.").arg(toString(code)));
        return nx_http::StatusCode::internalServerError;
    }

    QnWearableCameraReply reply;
    reply.id = camera.id;
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

    const auto camera = owner->resourcePool()->getResourceById(cameraId)
        .dynamicCast<QnWearableCameraResource>();

    if (!camera)
        return nx_http::StatusCode::invalidParameter;

    using namespace nx::vms::common::p2p::downloader;
    auto downloader = qnServerModule->findInstance<Downloader>();
    if (!downloader)
    {
        result.setError(QnJsonRestResult::CantProcessRequest,
            lit("DistributedFileDownloader is not initialized."));
        return nx_http::StatusCode::internalServerError;
    }

    std::unique_ptr<QFile> file = std::make_unique<QFile>(downloader->filePath(uploadId));
    if (!file->open(QIODevice::ReadOnly))
    {
        result.setError(QnJsonRestResult::CantProcessRequest,
            lit("Couldn't open file \"%1\" for reading.").arg(uploadId));
        return nx_http::StatusCode::internalServerError;
    }

    nx::mediaserver_core::recorder::WearableArchiveSynchronizationTask task(
        qnServerModule,
        camera,
        std::move(file),
        startTimeMs);
    task.execute();
    downloader->deleteFile(uploadId);

    return nx_http::StatusCode::ok;
}
