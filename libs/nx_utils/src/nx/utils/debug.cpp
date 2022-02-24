// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug.h"

namespace nx::utils::debug {

static OnAboutToBlockHandler onAboutToBlockHandler;

OnAboutToBlockHandler setOnAboutToBlockHandler(
    OnAboutToBlockHandler handler)
{
    return std::exchange(onAboutToBlockHandler, std::move(handler));
}

void onAboutToBlock()
{
    if (onAboutToBlockHandler)
        onAboutToBlockHandler();
}

} // namespace nx::utils::debug
