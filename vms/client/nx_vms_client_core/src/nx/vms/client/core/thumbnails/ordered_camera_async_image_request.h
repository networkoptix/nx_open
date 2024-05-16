// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/thumbnails/camera_async_image_request.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

class OrderedCameraAsyncImageRequest: public AsyncImageResult
{
    using base_type = AsyncImageResult;

public:
    using CameraStream = CameraAsyncImageRequest::CameraStream;

    OrderedCameraAsyncImageRequest(
        const QnVirtualCameraResourcePtr& camera,
        int maximumSize, //< Zero or negative for no limit.
        CameraStream stream = CameraStream::undefined,
        std::chrono::milliseconds timestamp = std::chrono::milliseconds(-1),
        QObject* parent = nullptr);

    virtual ~OrderedCameraAsyncImageRequest() override;

    void request();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
