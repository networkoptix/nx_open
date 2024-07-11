// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "image_request.h"

#include <nx/utils/datetime.h>

namespace nx::api {

bool CameraImageRequest::isSpecialTimeValue(std::chrono::milliseconds value)
{
    return value == nx::api::ImageRequest::kLatestThumbnail
        || value.count() == DATETIME_NOW;
}

} // namespace nx::api
