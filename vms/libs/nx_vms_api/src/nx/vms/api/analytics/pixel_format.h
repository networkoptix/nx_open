// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/json/flags.h>

namespace nx::vms::api::analytics {

// TODO: Move this file to a server-only library when manifest classes are moved the same way;
// replace with nx::sdk::analytics::IUncompressedFramePixelFormat which values should be
// changed to powers of 2 (to make it flag-capable).
NX_REFLECTION_ENUM_CLASS(PixelFormat,
    undefined = 0,
    yuv420 = 1 << 0,
    argb = 1 << 1,
    abgr = 1 << 2,
    rgba = 1 << 3,
    bgra = 1 << 4,
    rgb = 1 << 5,
    bgr = 1 << 6
)

Q_DECLARE_FLAGS(PixelFormats, PixelFormat)
Q_DECLARE_OPERATORS_FOR_FLAGS(PixelFormats)

} // namespace nx::vms::api::analytics
