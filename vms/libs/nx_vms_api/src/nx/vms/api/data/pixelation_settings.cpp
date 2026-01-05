// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pixelation_settings.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/compare.h>

namespace nx::vms::api {

bool ObjectTypeSettings::operator==(const ObjectTypeSettings& other) const
{
    return reflect::equals(*this, other);
}

bool ObjectTypeSettings::empty() const
{
    return !isAllObjectTypes && objectTypeIds.empty();
}

bool ObjectTypeSettings::contains(const QString& typeId) const
{
    return isAllObjectTypes || objectTypeIds.contains(typeId);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectTypeSettings, (json), ObjectTypeSettings_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PixelationSettings, (json), PixelationSettings_Fields)

QByteArray ObjectTypeSettings::toString() const
{
    return QJson::serialized(this);
}

bool PixelationSettings::operator==(const PixelationSettings& other) const
{
    return reflect::equals(*this, other);
}

QByteArray PixelationSettings::toString() const
{
    return QJson::serialized(this);
}

bool PixelationSettings::isPixelationRequiredForCamera(nx::Uuid cameraId) const
{
    if (excludeCameraIds.contains(cameraId))
        return false;

    return !empty();
}

} // namespace nx::vms::api
