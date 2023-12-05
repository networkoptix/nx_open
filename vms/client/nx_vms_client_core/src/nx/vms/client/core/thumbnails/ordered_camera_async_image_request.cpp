// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ordered_camera_async_image_request.h"

#include <nx/vms/client/core/common/utils/request_queue.h>

namespace {

using namespace nx::vms::client::core;

RequestQueue& requests()
{
    static const int kMaxDownloadRequestsCount = 8; //< Prevents remote requests queue overflow.
    static RequestQueue requestsQueue(kMaxDownloadRequestsCount);
    return requestsQueue;
}

} // namespace

namespace nx::vms::client::core {

struct OrderedCameraAsyncImageRequest::Private
{
    const QnVirtualCameraResourcePtr camera;
    const int maximumSize;
    const CameraStream stream;
    const std::chrono::milliseconds timestamp;

    std::unique_ptr<CameraAsyncImageRequest> imageResult;
};

//-------------------------------------------------------------------------------------------------

OrderedCameraAsyncImageRequest::OrderedCameraAsyncImageRequest(
    const QnVirtualCameraResourcePtr& camera,
    int maximumSize,
    CameraStream stream,
    std::chrono::milliseconds timestamp,
    QObject* parent)
    :
    base_type(parent),
    d(new Private{camera, maximumSize, stream, timestamp})
{
    requests().addRequest(
        [this](const auto callback)
        {
            d->imageResult.reset(new CameraAsyncImageRequest(
                d->camera, d->maximumSize, d->stream, d->timestamp));

            const auto handleReady =
                [this, callback]()
            {
                setImage(d->imageResult->image());
                callback();
            };

            if (d->imageResult->isReady())
                handleReady();
            else
                connect(d->imageResult.get(), &AsyncImageResult::ready, this, handleReady);
        });
}

OrderedCameraAsyncImageRequest::~OrderedCameraAsyncImageRequest()
{
}

} // namespace nx::vms::client::core
