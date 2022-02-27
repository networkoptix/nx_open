// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <api/helpers/multiserver_request_data.h>
#include <nx/api/mediaserver/image_request.h>

namespace nx::network::rest { class Params; }

struct NX_VMS_COMMON_API QnThumbnailRequestData: QnMultiserverRequestData
{
    QnThumbnailRequestData() = default;
    QnThumbnailRequestData(const nx::api::CameraImageRequest& request):
        request(request)
    {
    }

    QnThumbnailRequestData(nx::api::CameraImageRequest&& request):
        request(request)
    {
    }

    nx::api::CameraImageRequest request;

    virtual void loadFromParams(
        QnResourcePool* resourcePool, const nx::network::rest::Params& params) override;
    virtual nx::network::rest::Params toParams() const override;
    std::optional<QString> getError() const;
};
