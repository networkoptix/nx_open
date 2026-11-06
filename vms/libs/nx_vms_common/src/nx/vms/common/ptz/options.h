// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/fusion/model_functions_fwd.h>

#include "type.h"

namespace nx::vms::common {
namespace ptz {

struct Options
{
    Type type = Type::operational;

    /**
     * Optional movement speed in [0, 1]; fisheye positioning also accepts values above 1 for
     * immediate movement. Omission uses the operation's default. Unsupported relative movement
     * speed overrides are ignored.
     */
    std::optional<double> speed;
};
#define PtzOptions_Fields (type)(speed)
QN_FUSION_DECLARE_FUNCTIONS(Options, (json), NX_VMS_COMMON_API)

} // namespace ptz
} // namespace nx::vms::common
