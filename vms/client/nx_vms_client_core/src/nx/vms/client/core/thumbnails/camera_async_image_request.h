// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

#include "async_image_result.h"

namespace nx::api { struct CameraImageRequest; }

namespace nx::vms::client::core {

class CameraAsyncImageRequest: public AsyncImageResult, public RemoteConnectionAware
{
    Q_OBJECT
    using base_type = AsyncImageResult;

public:
    enum class CameraStream
    {
        undefined = -1,
        primary = 0,
        secondary
    };
    Q_ENUM(CameraStream)

public:
    explicit CameraAsyncImageRequest(
        const nx::api::CameraImageRequest& requestParams,
        QObject* parent = nullptr);

    explicit CameraAsyncImageRequest(
        const QnVirtualCameraResourcePtr& camera,
        int maximumSize, //< Zero or negative for no limit.
        CameraStream stream = CameraStream::undefined,
        QObject* parent = nullptr);

    virtual ~CameraAsyncImageRequest() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

QString toString(CameraAsyncImageRequest::CameraStream value);

} // namespace nx::vms::client::core
