// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dewarping_types.h"

#include <nx/reflect/string_conversion.h>

namespace nx::vms::api::dewarping {

NX_VMS_API std::ostream& operator<<(std::ostream& os, FisheyeCameraMount value)
{
    return os << nx::reflect::toString(value);
}

NX_VMS_API std::ostream& operator<<(std::ostream& os, CameraProjection value)
{
    return os << nx::reflect::toString(value);
}

NX_VMS_API std::ostream& operator<<(std::ostream& os, ViewProjection value)
{
    return os << nx::reflect::toString(value);
}

} // namespace nx::vms::api::dewarping
