// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "objects_bucket.h"

namespace nx::vms::client::mobile {
namespace timeline {

QVariant ObjectData::getTags() const
{
    return tags ? QVariant::fromValue(*tags) : QVariant{};
}

QVariant ObjectData::getAttributes() const
{
    return attributes ? QVariant::fromValue(*attributes) : QVariant{};
}

bool MultiObjectData::operator==(const MultiObjectData& other) const
{
    return caption == other.caption
        && description == other.description
        && iconPaths == other.iconPaths
        && imagePaths == other.imagePaths
        && positionMs == other.positionMs
        && count == other.count
        && perObjectData == other.perObjectData;
}

bool MultiObjectData::operator!=(const MultiObjectData& other) const
{
    return !operator==(other);
}

MultiObjectData ObjectBucket::getData() const
{
    return data.value_or(MultiObjectData{});
}

} // namespace timeline
} // namespace nx::vms::client::mobile
