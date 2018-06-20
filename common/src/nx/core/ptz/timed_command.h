#pragma once

#include <vector>
#include <chrono>

#include <nx/core/ptz/vector.h>

namespace nx {
namespace core {
namespace ptz {

struct TimedCommand
{
    nx::core::ptz::Vector command;
    std::chrono::milliseconds duration;
};

using CommandSequence = std::vector<TimedCommand>;

} // namespace ptz
} // namespace core
} // namespace nx
