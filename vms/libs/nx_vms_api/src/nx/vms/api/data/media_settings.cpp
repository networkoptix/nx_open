// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_settings.h"

namespace nx::vms::api {

MediaSettings::ValidationResult MediaSettings::validateMediaSettings() const
{
    if (stream != nx::vms::api::StreamIndex::undefined
        && stream != nx::vms::api::StreamIndex::primary
        && stream != nx::vms::api::StreamIndex::secondary)
    {
        return ValidationResult::invalidStreamIndex;
    }

    if (rotation != "auto"
        && rotation != "0"
        && rotation != "90"
        && rotation != "180"
        && rotation != "270")
    {
        return ValidationResult::invalidRotation;
    }

    if (dewarpingPanofactor != 1 && dewarpingPanofactor != 2 && dewarpingPanofactor != 4)
        return ValidationResult::invalidDewarpingPanofactor;

    if (zoom)
    {
        for (const auto value: {zoom->x(), zoom->y(), zoom->width(), zoom->height()})
        {
            if (value < 0.0 || value > 1.0)
                return ValidationResult::invalidZoom;
        }
    }

    return ValidationResult::isValid;
}

bool MediaSettings::isValid() const
{
    return validateMediaSettings() == ValidationResult::isValid;
}

bool MediaSettings::isLiveRequest() const
{
    return !positionUs;
}

} // namespace nx::vms::api
