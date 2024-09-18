// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_settings.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

// For unit test.
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MediaSettings, (json), MediaSettings_Fields)

bool MediaSettings::isValid() const
{
    if (id.isNull())
        return false;
    if (stream != nx::vms::api::StreamIndex::undefined
        && stream != nx::vms::api::StreamIndex::primary
        && stream != nx::vms::api::StreamIndex::secondary)
    {
        return false;
    }
    if (rotation != "auto"
        && rotation != "0"
        && rotation != "90"
        && rotation != "180"
        && rotation != "270")
    {
        return false;
    }
    auto validateFloat = [](double value) -> bool
        {
            return value >= 0.0 && value <= 1.0;
        };

    if (dewarpingPanofactor != 1 && dewarpingPanofactor != 2 && dewarpingPanofactor != 4)
        return false;
    if (zoom)
    {
        for (const auto value: {zoom->x(), zoom->y(), zoom->width(), zoom->height()})
            if (!validateFloat(value))
                return false;
    }
    return true;
}

} // namespace nx::vms::api
