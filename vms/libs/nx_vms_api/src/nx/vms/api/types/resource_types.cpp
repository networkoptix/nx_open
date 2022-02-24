// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_types.h"

#include <nx/reflect/string_conversion.h>

namespace nx::vms::api {

std::ostream& operator<<(std::ostream& os, StreamQuality value)
{
    return os << nx::reflect::toString(value);
}

std::ostream& operator<<(std::ostream& os, RecordingType value)
{
    return os << nx::reflect::toString(value);
}

std::ostream& operator<<(std::ostream& os, RecordingMetadataType value)
{
    return os << nx::reflect::toString(value);
}

std::ostream& operator<<(std::ostream& os, RecordingMetadataTypes value)
{
    return os << nx::reflect::toString(value);
}

} // namespace nx::vms::api
