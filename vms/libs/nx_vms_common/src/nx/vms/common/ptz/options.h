// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include "type.h"

namespace nx::vms::common {
namespace ptz {

struct Options
{
    Type type = Type::operational;
};
#define PtzOptions_Fields (type)
QN_FUSION_DECLARE_FUNCTIONS(Options, (json), NX_VMS_COMMON_API)

} // namespace ptz
} // namespace nx::vms::common
