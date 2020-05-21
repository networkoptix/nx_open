#pragma once

#include "named_point_sequence.h"

namespace nx::vms_server_plugins::analytics::vivotek {

struct NamedPolygon: NamedPointSequence
{
    using NamedPointSequence::NamedPointSequence;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
