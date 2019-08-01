#pragma once

#include <chrono>

namespace nx::analytics {

class IFrameInfo
{
public:
    virtual ~IFrameInfo() = default;
    virtual std::chrono::microseconds timestamp() const = 0;
};

} // namespace nx::analytics
