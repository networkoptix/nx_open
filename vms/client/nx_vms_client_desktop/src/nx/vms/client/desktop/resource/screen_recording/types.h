// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::desktop::screen_recording {

NX_REFLECTION_ENUM_CLASS(CaptureMode,
    fullScreen,
    fullScreenNoAero,
    window
)

NX_REFLECTION_ENUM_CLASS(Quality,
    best,
    balanced,
    performance
)

NX_REFLECTION_ENUM_CLASS(Resolution,
    native,
    quarterNative,
    _1920x1080,
    _1280x720,
    _640x480,
    _320x240
)

} // namespace nx::vms::client::desktop::screen_recording
