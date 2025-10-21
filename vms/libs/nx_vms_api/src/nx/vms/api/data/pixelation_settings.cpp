// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pixelation_settings.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/compare.h>

namespace nx::vms::api {

bool PixelationSettings::operator==(const PixelationSettings& other) const
{
    return nx::reflect::equals(*this, other);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PixelationSettings, (json), PixelationSettings_Fields)

QByteArray PixelationSettings::toString() const
{
    return QJson::serialized(this);
}

bool PixelationSettings::isPixelationRequiredForCamera(nx::Uuid cameraId) const
{
    if (excludeCameraIds.contains(cameraId))
        return false;

    return isAllObjectTypes || !objectTypeIds.empty();
}

} // namespace nx::vms::api
