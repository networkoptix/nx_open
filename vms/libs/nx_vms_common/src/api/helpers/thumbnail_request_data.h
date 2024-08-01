// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <api/helpers/multiserver_request_data.h>
#include <nx/api/mediaserver/image_request.h>

namespace nx::network::rest { class Params; }

struct NX_VMS_COMMON_API QnThumbnailRequestData: QnMultiserverRequestData
{
    // New API endpoints should use distinct types instead.
    enum class RequestType
    {
        cameraThumbnail,
        analyticsTrackBestShot,
        analyticsTrackTitle,
    };

    QnThumbnailRequestData(RequestType type):
        type(type)
    {}

    QnThumbnailRequestData(nx::api::CameraImageRequest request, RequestType type):
        request(std::move(request)),
        type(type)
    {
    }

    nx::api::CameraImageRequest request;
    RequestType type;

    static QnThumbnailRequestData loadFromParams(
        QnResourcePool *resourcePool, const nx::network::rest::Params& params, RequestType requestType)
    {
        QnThumbnailRequestData data(requestType);
        data.loadFromParams(resourcePool, params);
        return data;
    }

    virtual nx::network::rest::Params toParams() const override;
    std::optional<QString> getError() const;

private:
    // Explicitly disabling the possibility to call QnMultiserverRequestData, since it will not
    // initialize the `.type` field.
    virtual void loadFromParams(
        QnResourcePool* resourcePool, const nx::network::rest::Params& params) override;
};
