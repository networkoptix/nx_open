// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QRectF>
#include <QtCore/QSize>

#include <core/resource/resource_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/vms/api/data/thumbnail_filter.h>

namespace nx::api {

using ImageRequest = nx::vms::api::ThumbnailFilter;

struct ResourceImageRequest: ImageRequest
{
    QnUuid objectTrackId;
    QnResourcePtr resource;
};

struct NX_VMS_COMMON_API CameraImageRequest: ImageRequest
{
    QnUuid objectTrackId;
    QnVirtualCameraResourcePtr camera;

    CameraImageRequest() = default;

    CameraImageRequest(
        const QnVirtualCameraResourcePtr& camera,
        const ImageRequest& request,
        const QnUuid& objectTrackId = {})
        :
        ImageRequest(request),
        objectTrackId(objectTrackId),
        camera(camera)
    {
    }

    /** Check if value is "special" - DATETIME_NOW or negative. */
    static bool isSpecialTimeValue(std::chrono::microseconds value);
};

} // namespace nx::api

Q_DECLARE_METATYPE(nx::api::ImageRequest::StreamSelectionMode)
