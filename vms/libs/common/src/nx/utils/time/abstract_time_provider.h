#pragma once

#include <chrono>

namespace nx::utils::time {

class AbstractTimeProvider
{
public:
    virtual ~AbstractTimeProvider() {}

    virtual std::chrono::microseconds currentTime() const = 0;
};

} // namespace nx::utils::time
