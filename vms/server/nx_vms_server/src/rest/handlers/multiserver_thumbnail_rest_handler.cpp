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
#include <nx/utils/scope_guard.h>
#include <media_server/media_server_module.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/utils/switch.h>

namespace {

std::array<QByteArray, 2> extraImageHeaders =
{
    Qn::FRAME_TIMESTAMP_US_HEADER_NAME,
    Qn::FRAME_ENCODED_BY_PLUGIN
};

using namespace nx::api;

static QByteArray toMimeSubtype(ImageRequest::ThumbnailFormat format, AVPixelFormat pixelFormat)
{
    return nx::utils::switch_(format,
        ImageRequest::ThumbnailFormat::jpg, [] { return "jpg"; },
        ImageRequest::ThumbnailFormat::tif, [] { return "tiff"; },
        ImageRequest::ThumbnailFormat::png, [] { return "png"; },
        ImageRequest::ThumbnailFormat::raw, [&] { return toString(pixelFormat).toUtf8(); }
    );
}

} // namespace

const QString QnMultiserverThumbnailRestHandler::kDefaultPath = "ec2/analyticsTrackBestShot";

QnMultiserverThumbnailRestHandler::QnMultiserverThumbnailRestHandler(
    QnMediaServerModule* serverModule, const QString& path)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    m_path(path)
{
}

int QnMultiserverThumbnailRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result, QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    QThread::currentThread()->setPriority(QThread::LowPriority);
    auto threadPriorityGuard = nx::utils::makeScopeGuard(
        []()
        {
            QThread::currentThread()->setPriority(QThread::NormalPriority);
        });

    const auto request = QnMultiserverRequestData::fromParams<QnThumbnailRequestData>(
        processor->commonModule()->resourcePool(), params);

    if (!request.request.camera)
    {
        QnJsonRestResult::writeError(
            &result, QnRestResult::InvalidParameter, "Parameter cameraId is missing or invalid");
        return nx::network::http::StatusCode::unprocessableEntity;
    }

    const auto& imageRequest = request.request;

    const auto requiredPermission =
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
    nx::network::http::HttpHeaders extraHeaders;
    const auto httpResult = getScreenshot(processor->commonModule(), request, result, contentType,
        ownerPort, &extraHeaders);

    if (httpResult == nx::network::http::StatusCode::ok)
    {
        auto& headers = processor->response()->headers;
        for (const auto& newHeader: extraHeaders)
        {
            nx::network::http::insertHeader(&headers, nx::network::http::HttpHeader(
                newHeader.first, newHeader.second));
        }
    }
    return httpResult;
}

int QnMultiserverThumbnailRestHandler::getScreenshot(
    QnCommonModule* commonModule,
    const QnThumbnailRequestData& request,
    QByteArray& result,
    QByteArray& contentType,
    int ownerPort,
    qint64* frameTimestamsUsec)
{
    nx::network::http::HttpHeaders extraHeaders;
    int statusCode =
        getScreenshot(commonModule, request, result, contentType, ownerPort, &extraHeaders);
    if (frameTimestamsUsec)
    {
        const auto it = extraHeaders.find(Qn::FRAME_TIMESTAMP_US_HEADER_NAME);
        if (it != extraHeaders.end())
            *frameTimestamsUsec = it->second.toLongLong(); //< 0 if conversion fails.
    }
    return statusCode;
}

int QnMultiserverThumbnailRestHandler::getScreenshot(
    QnCommonModule* commonModule,
    const QnThumbnailRequestData &request,
    QByteArray& result,
    QByteArray& contentType,
    int ownerPort,
    nx::network::http::HttpHeaders* outExtraHeaders)
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
        ? getThumbnailLocal(request, result, contentType, outExtraHeaders)
        : getThumbnailRemote(server, request, result, contentType, ownerPort, outExtraHeaders);
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
    const QnThumbnailRequestData& request,
    QByteArray& result,
    QByteArray& contentType,
    nx::network::http::HttpHeaders* outExtraHeaders) const
{
    if (!request.request.objectTrackId.isNull())
        return getThumbnailForAnalyticsTrack(request, result, contentType, outExtraHeaders);
    return getThumbnailFromArchive(request, result, contentType, outExtraHeaders);
}

int QnMultiserverThumbnailRestHandler::getThumbnailForAnalyticsTrack(
    const QnThumbnailRequestData& request,
    QByteArray& result,
    QByteArray& contentType,
    nx::network::http::HttpHeaders* outExtraHeaders) const
{
    result = QByteArray();
    contentType = QByteArray();

    QnGetImageHelper helper(serverModule());
    auto bestShot = helper.getTrackBestShot(request.request);
    if (!bestShot)
    {
        NX_VERBOSE(this, "Best shot for the track with id %1 is not found",
            request.request.objectTrackId);
        // There is no best shot in the DB for such a track, so we're trying to use other
        // parameters to fetch it directly from the archive.
        return getThumbnailFromArchive(request, result, contentType, outExtraHeaders);
    }

    if (outExtraHeaders)
    {
        nx::network::http::insertHeader(outExtraHeaders, nx::network::http::HttpHeader(
            Qn::FRAME_TIMESTAMP_US_HEADER_NAME, QByteArray::number(bestShot->timestampUs)));
    }

    if (!bestShot->image.isEmpty())
    {
        if (outExtraHeaders)
        {
            nx::network::http::insertHeader(outExtraHeaders, nx::network::http::HttpHeader(
                Qn::FRAME_ENCODED_BY_PLUGIN, QnLexical::serialized(true).toLatin1()));
        }

        NX_VERBOSE(this, "Found frame in analytics cache for track %1", request.request.objectTrackId);
        result = bestShot->image.imageData;
        contentType = bestShot->image.imageDataFormat;
        return nx::network::http::StatusCode::ok;
    }

    NX_VERBOSE(this, "Best shot found without image data for track %1", request.request.objectTrackId);

    QnThumbnailRequestData newRequest;
    newRequest.format = request.format;
    auto camera = serverModule()->resourcePool()->getResourceById<QnVirtualCameraResource>(bestShot->deviceId);
    if (!camera)
    {
        NX_VERBOSE(this, "Best shot for track %1 is not found. Camera %2 is not fould",
            request.request.objectTrackId,
            bestShot->deviceId);
        return nx::network::http::StatusCode::noContent;
    }
    newRequest.request.camera = camera;
    newRequest.request.crop = bestShot->rect;
    newRequest.request.streamSelectionMode =
        (bestShot->streamIndex == nx::vms::api::StreamIndex::primary)
            ? CameraImageRequest::StreamSelectionMode::forcedPrimary
            : CameraImageRequest::StreamSelectionMode::forcedSecondary;
    newRequest.request.usecSinceEpoch = bestShot->timestampUs;
    newRequest.request.roundMethod = ImageRequest::RoundMethod::precise;

    return getThumbnailFromArchive(newRequest, result, contentType, outExtraHeaders);
}

int QnMultiserverThumbnailRestHandler::getThumbnailFromArchive(
    const QnThumbnailRequestData &request,
    QByteArray& result,
    QByteArray& contentType,
    nx::network::http::HttpHeaders* outExtraHeaders) const
{
    QnGetImageHelper helper(serverModule());
    CLVideoDecoderOutputPtr outFrame = helper.getImage(request.request);

    if (!outFrame)
    {
        result = QByteArray();
        contentType = QByteArray();
        return nx::network::http::StatusCode::noContent;
    }

    if (outExtraHeaders)
    {
        nx::network::http::insertHeader(
            outExtraHeaders,
            nx::network::http::HttpHeader(
                Qn::FRAME_TIMESTAMP_US_HEADER_NAME,
                QByteArray::number((qint64) outFrame.data()->pkt_dts)));
    }

    const QByteArray mimeSubtype =
        toMimeSubtype(request.request.imageFormat, (AVPixelFormat) outFrame->format);

    serverModule()->commonModule()->metrics()->thumbnails()++;
    if (request.request.imageFormat == nx::api::CameraImageRequest::ThumbnailFormat::jpg)
    {
        QByteArray encodedData = helper.encodeImage(outFrame, mimeSubtype);
        result.append(encodedData);
    }
    else if (request.request.imageFormat == nx::api::CameraImageRequest::ThumbnailFormat::raw)
    {
        result = outFrame->rawData();
    }
    else
    {
        // Prepare image using Qt.
        QImage image = outFrame->toImage();
        QBuffer output(&result);
        image.save(&output, mimeSubtype);
    }

    if (result.isEmpty())
    {
        NX_WARNING(this, lm("Can't encode image to '%1' format. Image size: %2x%3")
            .arg(QString::fromUtf8(mimeSubtype))
            .arg(outFrame->width)
            .arg(outFrame->height));

        return makeError(
            nx::network::http::StatusCode::unsupportedMediaType
            , lit("Unsupported image format '%1'").arg(QString::fromUtf8(mimeSubtype))
            , &result
            , &contentType
            , request.format
            , request.extraFormatting);
    }

    contentType = QByteArray("image/") + mimeSubtype;
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
    const QString& urlPath,
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
    nx::network::http::HttpHeaders* outExtraHeaders) const
{
    const auto response = downloadImage(server, request, m_path, ownerPort);
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
        if (outExtraHeaders)
        {
            for (const auto& headerName: extraImageHeaders)
            {
                const auto it = response.httpHeaders.find(headerName);
                if (it != response.httpHeaders.end())
                {
                    nx::network::http::insertHeader(outExtraHeaders, nx::network::http::HttpHeader(
                        it->first, it->second));
                }
            }
        }
    }

    return response.httpStatus;
}
