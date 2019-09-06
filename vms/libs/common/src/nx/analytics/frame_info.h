#pragma once

#include <chrono>

namespace nx::analytics {

struct FrameInfo
{
    std::chrono::microseconds timestamp;
};

} // namespace nx::analytics
