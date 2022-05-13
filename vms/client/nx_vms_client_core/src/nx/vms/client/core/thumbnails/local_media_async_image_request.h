// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <core/resource/resource_fwd.h>

#include <nx/utils/impl_ptr.h>

#include "async_image_result.h"

namespace nx::api { struct CameraImageRequest; }

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API LocalMediaAsyncImageRequest: public AsyncImageResult
{
    Q_OBJECT
    using base_type = AsyncImageResult;

public:
    explicit LocalMediaAsyncImageRequest(
        const QnResourcePtr& resource,
        std::chrono::milliseconds position,
        int maximumSize, //< Zero or negative for no limit.
        QObject* parent = nullptr);

    virtual ~LocalMediaAsyncImageRequest() override;

    static constexpr std::chrono::milliseconds kMiddlePosition{-1};

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
