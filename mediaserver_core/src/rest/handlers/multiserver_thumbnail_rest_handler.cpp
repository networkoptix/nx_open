#include "multiserver_thumbnail_rest_handler.h"

#include <QtGui/QImage>

#include <api/helpers/thumbnail_request_data.h>

#include <camera/get_image_helper.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>

#include <network/tcp_listener.h>

#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <rest/server/rest_connection_processor.h>

#include <utils/media/frame_info.h>

namespace
{
    QString urlPath;
}

QnMultiserverThumbnailRestHandler::QnMultiserverThumbnailRestHandler( const QString& path )
{
    // todo: remove this variable
    if (!path.isEmpty())
        urlPath = path;
}

int QnMultiserverThumbnailRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result, QByteArray& contentType,
    const QnRestConnectionProcessor* processor )
{
    auto request = QnMultiserverRequestData::fromParams<QnThumbnailRequestData>(processor->commonModule()->resourcePool(), params);
    const auto ownerPort = processor->owner()->getPort();
    return getScreenshot(processor->commonModule(), request, result, contentType, ownerPort);
}

int QnMultiserverThumbnailRestHandler::getScreenshot(
    QnCommonModule* commonModule,
    const QnThumbnailRequestData &request,
    QByteArray& result,
    QByteArray& contentType,
    int ownerPort)
{
    if (request.camera && !request.camera->hasVideo(nullptr))
    {
        return makeError(
            nx_http::StatusCode::badRequest
            , lit("Camera has no video")
            , &result
            , &contentType
            , request.format
            , request.extraFormatting);
    }

    if (!request.isValid())
    {
        return makeError(
            nx_http::StatusCode::badRequest
            , lit("Invalid request") // TODO: #GDM think about more detailed error
            , &result
            , &contentType
            , request.format
            , request.extraFormatting);
    }

    auto server = targetServer(commonModule, request);
    if (!server || server->getId() == commonModule->moduleGUID() || server->getStatus() != Qn::Online)
        return getThumbnailLocal(request, result, contentType);

    return getThumbnailRemote(server, request, result, contentType, ownerPort);
}

QnMediaServerResourcePtr QnMultiserverThumbnailRestHandler::targetServer(
    QnCommonModule* commonModule,
    const QnThumbnailRequestData &request ) const
{
    auto currentServer = commonModule->resourcePool()->getResourceById<QnMediaServerResource>(commonModule->moduleGUID());

    if (request.isLocal)
        return currentServer;

    if (QnThumbnailRequestData::isSpecialTimeValue(request.msecSinceEpoch))
        return request.camera->getParentServer();

    return commonModule->cameraHistoryPool()->getMediaServerOnTimeSync(request.camera, request.msecSinceEpoch);
}

int QnMultiserverThumbnailRestHandler::getThumbnailLocal( const QnThumbnailRequestData &request, QByteArray& result, QByteArray& contentType ) const
{
    static const qint64 kUsecPerMs = 1000;

    qint64 timeUSec = QnThumbnailRequestData::isSpecialTimeValue(request.msecSinceEpoch)
        ? request.msecSinceEpoch
        : request.msecSinceEpoch * kUsecPerMs;

    CLVideoDecoderOutputPtr outFrame = QnGetImageHelper::getImage(
        request.camera,
        timeUSec,
        request.size,
        request.roundMethod,
        request.rotation,
        request.aspectRatio);

    if (!outFrame)
    {
        result = QByteArray();
        contentType = QByteArray();
        return nx_http::StatusCode::noContent;
    }

    QByteArray imageFormat = QnLexical::serialized<QnThumbnailRequestData::ThumbnailFormat>(request.imageFormat).toUtf8();

    if (request.imageFormat == QnThumbnailRequestData::JpgFormat)
    {
        QByteArray encodedData = QnGetImageHelper::encodeImage(outFrame, imageFormat);
        result.append(encodedData);
    }
    else if (request.imageFormat == QnThumbnailRequestData::RawFormat)
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Method is not implemeted");
        // TODO: #rvasilenko implement me!!!
    }
    else
    {
        // prepare image using QT
        QImage image = outFrame->toImage();
        QBuffer output(&result);
        image.save(&output, imageFormat);
    }

    if (result.isEmpty())
    {
        return makeError(
            nx_http::StatusCode::badRequest
            , lit("Unsupported image format '%1'").arg(QString::fromUtf8(imageFormat))
            , &result
            , &contentType
            , request.format
            , request.extraFormatting);
    }

    contentType = QByteArray("image/") + imageFormat;
    return nx_http::StatusCode::ok;
}


int QnMultiserverThumbnailRestHandler::getThumbnailRemote(
    const QnMediaServerResourcePtr &server,
    const QnThumbnailRequestData &request,
    QByteArray& result,
    QByteArray& contentType,
    int ownerPort ) const
{
    typedef QnMultiserverRequestContext<QnThumbnailRequestData> QnThumbnailRequestContext;

    QnThumbnailRequestContext context(request, ownerPort);
    NX_ASSERT(!request.isLocal, Q_FUNC_INFO, "Local request must be processed before");

    nx::utils::Url apiUrl(server->getApiUrl());
    apiUrl.setPath(L'/' + urlPath);

    QnThumbnailRequestData modifiedRequest(request);
    modifiedRequest.makeLocal();
    apiUrl.setQuery(modifiedRequest.toUrlQuery());
    // TODO: #rvasilenko implement raw format here

    auto requestCompletionFunc = [&context, &result] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody ) {

        if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok ) {
            result = msgBody;
        }

        context.executeGuarded([&context]()
        {
            context.requestProcessed();
        });
    };
    runMultiserverDownloadRequest(server->commonModule()->router(), apiUrl, server, requestCompletionFunc, &context);
    context.waitForDone();

    QByteArray imageFormat = QnLexical::serialized<QnThumbnailRequestData::ThumbnailFormat>(request.imageFormat).toUtf8();
    if (result.isEmpty())
    {
        return makeError(
            nx_http::StatusCode::badRequest
            , lit("Unsupported image format '%1'").arg(QString::fromUtf8(imageFormat))
            , &result
            , &contentType
            , request.format
            , request.extraFormatting);
    }

    contentType = QByteArray("image/") + imageFormat;
    return nx_http::StatusCode::ok;
}
