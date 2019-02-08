#include "multiserver_thumbnail_rest_handler.h"

#include <chrono>

#include <QtGui/QImage>

#include <api/helpers/thumbnail_request_data.h>

#include <camera/get_image_helper.h>

#include <common/common_module.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <network/tcp_listener.h>

#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <rest/server/rest_connection_processor.h>

#include <utils/media/frame_info.h>
#include <nx/utils/log/log_main.h>

namespace {

static QString urlPath;

} // namespace

QnMultiserverThumbnailRestHandler::QnMultiserverThumbnailRestHandler(
    QnMediaServerModule* serverModule, const QString& path):
    nx::vms::server::ServerModuleAware(serverModule)
{
    if (!path.isEmpty())
        urlPath = path;
}

int QnMultiserverThumbnailRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result, QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    auto request = QnMultiserverRequestData::fromParams<QnThumbnailRequestData>(
        processor->commonModule()->resourcePool(), params);
    const auto& imageRequest = request.request;

    auto requiredPermission =
        nx::api::CameraImageRequest::isSpecialTimeValue(imageRequest.usecSinceEpoch)
        ? Qn::Permission::ViewLivePermission
        : Qn::Permission::ViewFootagePermission;

    if (!processor->commonModule()->resourceAccessManager()->hasPermission(
        processor->accessRights(), imageRequest.camera, requiredPermission))
    {
        return makeError(nx::network::http::StatusCode::forbidden,
            lit("Access denied: Insufficient access rights"),
            &result, &contentType, request.format, request.extraFormatting);
    }

    const auto ownerPort = processor->owner()->getPort();
    qint64 frameTimestampUsec = 0;
    auto httpResult = getScreenshot(processor->commonModule(), request, result, contentType,
        ownerPort, &frameTimestampUsec);

    if (httpResult == nx::network::http::StatusCode::ok)
    {
        auto& headers = processor->response()->headers;
        nx::network::http::insertHeader(&headers, nx::network::http::HttpHeader(
            Qn::FRAME_TIMESTAMP_US_HEADER_NAME, QByteArray::number(frameTimestampUsec)));
    }
    return httpResult;
}

int QnMultiserverThumbnailRestHandler::getScreenshot(
    QnCommonModule* commonModule,
    const QnThumbnailRequestData &request,
    QByteArray& result,
    QByteArray& contentType,
    int ownerPort,
    qint64* frameTimestamsUsec)
{
    const auto& imageRequest = request.request;

    if (imageRequest.camera && !imageRequest.camera->hasVideo(nullptr))
    {
        return makeError(
            nx::network::http::StatusCode::badRequest,
            lit("Camera has no video"),
            &result,
            &contentType,
            request.format,
            request.extraFormatting);
    }

    if (const auto error = request.getError())
    {
        return makeError(
            nx::network::http::StatusCode::badRequest,
            lit("Invalid request: ") + *error,
            &result,
            &contentType,
            request.format,
            request.extraFormatting);
    }

    auto server = targetServer(commonModule, request);
    bool loadLocal = !server || server->getId() == commonModule->moduleGUID()
        || server->getStatus() != Qn::Online;

    return (loadLocal)
        ? getThumbnailLocal(request, result, contentType, frameTimestamsUsec)
        : getThumbnailRemote(server, request, result, contentType, ownerPort, frameTimestamsUsec);
}

QnMediaServerResourcePtr QnMultiserverThumbnailRestHandler::targetServer(
    QnCommonModule* commonModule,
    const QnThumbnailRequestData &request ) const
{
    auto currentServer = commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
        commonModule->moduleGUID());

    if (request.isLocal)
        return currentServer;

    const auto& imageRequest = request.request;
    if (nx::api::CameraImageRequest::isSpecialTimeValue(imageRequest.usecSinceEpoch))
        return imageRequest.camera->getParentServer();

    using namespace std::chrono;
    return commonModule->cameraHistoryPool()->getMediaServerOnTimeSync(imageRequest.camera,
        duration_cast<milliseconds>(microseconds(imageRequest.usecSinceEpoch)).count());
}

int QnMultiserverThumbnailRestHandler::getThumbnailLocal(
    const QnThumbnailRequestData &request,
    QByteArray& result, 
    QByteArray& contentType, 
    qint64* frameTimestampUsec) const
{
    QnGetImageHelper helper(serverModule());
    CLVideoDecoderOutputPtr outFrame = helper.getImage(request.request);

    if (!outFrame)
    {
        result = QByteArray();
        contentType = QByteArray();
        return nx::network::http::StatusCode::noContent;
    }
    if(frameTimestampUsec)
        *frameTimestampUsec = static_cast<qint64>(outFrame.data()->pkt_dts);

    QByteArray imageFormat = QnLexical::serialized<nx::api::CameraImageRequest::ThumbnailFormat>(
        request.request.imageFormat).toUtf8();

    if (request.request.imageFormat == nx::api::CameraImageRequest::ThumbnailFormat::jpg)
    {
        QByteArray encodedData = helper.encodeImage(outFrame, imageFormat);
        result.append(encodedData);
    }
    else if (request.request.imageFormat == nx::api::CameraImageRequest::ThumbnailFormat::raw)
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Method is not implemented");
        // TODO: #rvasilenko implement me!!!
    }
    else
    {
        // Prepare image using Qt.
        QImage image = outFrame->toImage();
        QBuffer output(&result);
        image.save(&output, imageFormat);
    }

    if (result.isEmpty())
    {
        NX_WARNING(this, lm("Can't encode image to '%1' format. Image size: %2x%3")
            .arg(QString::fromUtf8(imageFormat))
            .arg(outFrame->width)
            .arg(outFrame->height));

        return makeError(
            nx::network::http::StatusCode::unsupportedMediaType
            , lit("Unsupported image format '%1'").arg(QString::fromUtf8(imageFormat))
            , &result
            , &contentType
            , request.format
            , request.extraFormatting);
    }

    contentType = QByteArray("image/") + imageFormat;
    return nx::network::http::StatusCode::ok;
}

struct DownloadResult
{
    SystemError::ErrorCode osStatus = SystemError::notImplemented;
    nx::network::http::StatusCode::Value httpStatus = nx::network::http::StatusCode::internalServerError;
    QByteArray body;
    nx::network::http::HttpHeaders httpHeaders;
};

static DownloadResult downloadImage(
    const QnMediaServerResourcePtr& server,
    const QnThumbnailRequestData& request,
    int ownerPort)
{
    NX_ASSERT(!request.isLocal, "Local request must be processed before");
    nx::utils::Url apiUrl(server->getApiUrl());
    apiUrl.setPath(L'/' + urlPath);

    QnMultiserverRequestContext<QnThumbnailRequestData> context(request, ownerPort);
    QnThumbnailRequestData modifiedRequest(request);
    modifiedRequest.makeLocal();
    apiUrl.setQuery(modifiedRequest.toUrlQuery());
    // TODO: #rvasilenko implement raw format here

    DownloadResult result;

    //[&] (SystemError::ErrorCode osStatus, int httpStatus, const nx::network::http::Response& response)

    auto requestCompletionFunc =
        [&l_result = result, &l_context = context](
            SystemError::ErrorCode osStatus,
            int httpStatus,
            nx::network::http::BufferType buffer,
            nx::network::http::HttpHeaders httpHeaders)
        {
            l_result.osStatus = osStatus;
            l_result.httpStatus = (nx::network::http::StatusCode::Value) httpStatus;
            l_result.body = std::move(buffer);
            l_result.httpHeaders = std::move(httpHeaders);
            l_context.executeGuarded([&l_context]() { l_context.requestProcessed(); });
        };

    runMultiserverDownloadRequest(
        server->commonModule()->router(),
        apiUrl,
        server,
        requestCompletionFunc,
        &context);

    context.waitForDone();
    NX_DEBUG(typeid(QnMultiserverThumbnailRestHandler),
        lm("Request to '%1' finithed (%2) http status %3").args(
            apiUrl, SystemError::toString(result.osStatus), result.httpStatus));

    return result;
}

int QnMultiserverThumbnailRestHandler::getThumbnailRemote(
    const QnMediaServerResourcePtr &server,
    const QnThumbnailRequestData &request,
    QByteArray& result,
    QByteArray& contentType,
    int ownerPort,
    qint64* frameTimestamsUsec) const
{
    const auto response = downloadImage(server, request, ownerPort);
    if (response.osStatus != SystemError::noError)
    {
        return makeError(nx::network::http::StatusCode::internalServerError,
            lit("Internal error: ") + SystemError::toString(response.osStatus),
            &result, &contentType, request.format, request.extraFormatting);
    }

    if (response.httpStatus == nx::network::http::StatusCode::unauthorized)
    {
        return makeError(nx::network::http::StatusCode::internalServerError,
            lit("Internal error: ") + nx::network::http::StatusCode::toString(response.httpStatus),
            &result, &contentType, request.format, request.extraFormatting);
    }

    if (!response.body.isEmpty())
    {
        result = std::move(response.body);
        contentType = (response.httpStatus == nx::network::http::StatusCode::ok)
            ? QByteArray("image/") + QnLexical::serialized(request.request.imageFormat).toUtf8()
            : QByteArray("application/json"); //< TODO: Provide content type from response.
        if (frameTimestamsUsec)
        {
            const auto it = response.httpHeaders.find(Qn::FRAME_TIMESTAMP_US_HEADER_NAME);
            if (it != response.httpHeaders.end())
            {
                *frameTimestamsUsec = it->second.toLongLong(); //< 0 if conversion fails.
            }
        }
    }

    return response.httpStatus;
}
