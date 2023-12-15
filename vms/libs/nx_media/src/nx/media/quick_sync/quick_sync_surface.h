// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vpl/mfx.h>

namespace nx::media::quick_sync {
class QuickSyncVideoDecoderImpl;
}

struct QuickSyncSurface
{
    mfxFrameSurface1* surface;
    // Contain reference to video decoder to ensure that it still alive
    std::weak_ptr<nx::media::quick_sync::QuickSyncVideoDecoderImpl> decoder;
};
