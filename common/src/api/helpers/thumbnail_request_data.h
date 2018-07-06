#pragma once

#include <nx/api/mediaserver/image_request.h>

#include <api/helpers/multiserver_request_data.h>

#include <utils/common/request_param.h>

struct QnThumbnailRequestData: QnMultiserverRequestData
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

    virtual void loadFromParams(QnResourcePool* resourcePool, const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    boost::optional<QString> getError() const;
};
