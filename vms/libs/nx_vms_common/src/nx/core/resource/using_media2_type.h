// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::core::resource {

/** To use or not to use Onvif::Media2 service for some operation on an onvif device. */
NX_REFLECTION_ENUM_CLASS(UsingOnvifMedia2Type,
    autoSelect = 0, //< An algorithm chooses it itself (probably with resource_data.json parameter).
    useIfSupported,
    neverUse
)

} // namespace nx::core::resource
