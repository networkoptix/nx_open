#pragma once

#include <deque>
#include <chrono>

#include <nx/core/ptz/vector.h>
#include <nx/core/ptz/options.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace core {
namespace ptz {

struct TimedCommand
{
    nx::core::ptz::Vector command;
    nx::core::ptz::Options options;
    std::chrono::milliseconds duration;
};
#define PtzTimedCommand_fields (command)(options)(duration)

QN_FUSION_DECLARE_FUNCTIONS(TimedCommand, (eq))

using CommandSequence = std::deque<TimedCommand>;

} // namespace ptz
} // namespace core
} // namespace nx
