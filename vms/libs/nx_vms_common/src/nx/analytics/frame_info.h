// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

namespace nx::analytics {

struct FrameInfo
{
    std::chrono::microseconds timestamp;
};

} // namespace nx::analytics
