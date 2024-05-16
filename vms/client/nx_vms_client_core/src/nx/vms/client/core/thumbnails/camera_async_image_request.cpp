// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_async_image_request.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QFutureWatcher>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_media_layout.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/enum_utils.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/common/utils/custom_thread_pool.h>
#include <nx/vms/client/core/system_context.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::core {

namespace {

QSize calculateSizeLimit(const QnVirtualCameraResourcePtr& camera, int maximumSize)
{
    if (!NX_ASSERT(camera))
        return {};

    if (maximumSize <= 0)
        return {};

    const auto layout = camera->getVideoLayout()->size();
    const auto ar = camera->aspectRatio();
    const bool vertical = ar.width() * layout.width() < ar.height() * layout.height();

    return vertical ? QSize(0, maximumSize) : QSize(maximumSize, 0);
}

nx::api::ImageRequest::StreamSelectionMode streamSelectionMode(
    CameraAsyncImageRequest::CameraStream stream)
{
    switch (stream)
    {
        case CameraAsyncImageRequest::CameraStream::undefined:
            return nx::api::ImageRequest::StreamSelectionMode::auto_;

        case CameraAsyncImageRequest::CameraStream::primary:
            return nx::api::ImageRequest::StreamSelectionMode::forcedPrimary;

        case CameraAsyncImageRequest::CameraStream::secondary:
            return nx::api::ImageRequest::StreamSelectionMode::forcedSecondary;
    }

    NX_ASSERT(false);
    return nx::api::ImageRequest::StreamSelectionMode::auto_;
}

nx::api::CameraImageRequest makeRequestParams(
    const QnVirtualCameraResourcePtr& camera,
    int maximumSize,
    CameraAsyncImageRequest::CameraStream stream,
    std::chrono::milliseconds timestamp)
{
    nx::api::CameraImageRequest requestParams;
    requestParams.camera = camera;
    requestParams.rotation = 0;
    requestParams.streamSelectionMode = streamSelectionMode(stream);
    requestParams.size = calculateSizeLimit(camera, maximumSize);
    requestParams.timestampMs = timestamp;
    return requestParams;
}

} // namespace

struct CameraAsyncImageRequest::Private
{
    QFutureWatcher<QImage> imageWatcher;
    rest::Handle requestId{};

    QThreadPool* threadPool() const
    {
        return CustomThreadPool::instance("CameraAsyncImageRequest_thread_pool");
    }
};

// ------------------------------------------------------------------------------------------------
// CameraAsyncImageRequest

CameraAsyncImageRequest::CameraAsyncImageRequest(
    const nx::api::CameraImageRequest& requestParams,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    auto systemContext = SystemContext::fromResource(requestParams.camera);
    if (!NX_ASSERT(requestParams.camera)
        || !requestParams.camera->hasVideo()
        || !NX_ASSERT(systemContext)
        || !NX_ASSERT(d->threadPool()))
    {
        setImage({});
        return;
    }

    const auto api = systemContext->connectedServerApi();
    if (!api)
    {
        setImage({});
        return;
    }

    connect(&d->imageWatcher, &QFutureWatcherBase::finished, this,
        [this]() { setImage(d->imageWatcher.future().result()); });

    const auto imageFormat = nx::reflect::toString(requestParams.format);

    const auto callback = nx::utils::guarded(this,
        [this, imageFormat](
            bool success,
            rest::Handle requestId,
            QByteArray imageData,
            const nx::network::http::HttpHeaders& headers)
        {
            if (!NX_ASSERT(requestId == d->requestId))
                return;

            // TODO: add error message processing.
            const bool isJsonError = nx::network::http::getHeaderValue(headers, "Content-Type")
                == Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json);

            if (!success || imageData.isEmpty() || isJsonError || !d->threadPool())
            {
                setImage({});
                return;
            }

            d->requestId = {};

            const nx::String timestampUs = nx::network::http::getHeaderValue(headers,
                Qn::FRAME_TIMESTAMP_US_HEADER_NAME);

            // Decompress image in another thread.
            d->imageWatcher.setFuture(QtConcurrent::run(d->threadPool(),
                [imageData, imageFormat, timestampUs]() -> QImage
                {
                    QImage image;
                    image.loadFromData(imageData, imageFormat.c_str());
                    image.setText(kTimestampUsKey, timestampUs);
                    return image;
                }));
        });

    NX_VERBOSE(this, "Requesting thumbnail for %1 (%2)", requestParams.camera->getName(),
        requestParams.camera->getId());

    d->requestId = api->cameraThumbnailAsync(requestParams, callback, thread());
    if (d->requestId == rest::Handle())
        setImage({});
};

CameraAsyncImageRequest::CameraAsyncImageRequest(
    const QnVirtualCameraResourcePtr& camera,
    int maximumSize,
    CameraStream stream,
    std::chrono::milliseconds timestamp,
    QObject* parent)
    :
    CameraAsyncImageRequest(makeRequestParams(camera, maximumSize, stream, timestamp), parent)
{
}

CameraAsyncImageRequest::~CameraAsyncImageRequest()
{
    // Required here for forward-declared scoped pointer destruction.
}

QString toString(CameraAsyncImageRequest::CameraStream value)
{
    return nx::utils::enumToShortString(value);
}

} // namespace nx::vms::client::core
