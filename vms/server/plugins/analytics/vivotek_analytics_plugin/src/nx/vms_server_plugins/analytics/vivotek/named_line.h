#pragma once

#include "named_point_sequence.h"

namespace nx::vms_server_plugins::analytics::vivotek {

struct NamedLine: NamedPointSequence
{
    using NamedPointSequence::NamedPointSequence;

    enum class Direction
    {
        any,
        leftToRight,
        rightToLeft,
    };
    Direction direction = Direction::any;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
