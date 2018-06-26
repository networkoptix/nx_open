#pragma once

#include <deque>
#include <chrono>

#include <nx/core/ptz/vector.h>
#include <nx/core/ptz/options.h>

namespace nx {
namespace core {
namespace ptz {

struct TimedCommand
{
    nx::core::ptz::Vector command;
    nx::core::ptz::Options options;
    std::chrono::milliseconds duration;
};

using CommandSequence = std::deque<TimedCommand>;

} // namespace ptz
} // namespace core
} // namespace nx
