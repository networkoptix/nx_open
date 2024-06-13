// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::desktop {

NX_REFLECTION_ENUM_CLASS(GraphicsApi,
    opengl,
    metal,
    direct3d11,
    vulkan,
    openvg,
    software
);

} // namespace nx::vms::client::desktop
