// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::media {

enum class HardwareAccelerationType
{
    none,
    quickSync,
    nvidia,
};

HardwareAccelerationType getHardwareAccelerationType();

} // namespace nx::media
