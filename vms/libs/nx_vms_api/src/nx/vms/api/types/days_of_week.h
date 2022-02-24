// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(DayOfWeek,
    monday    = 0,
    tuesday   = 1,
    wednesday = 2,
    thursday  = 3,
    friday    = 4,
    saturday  = 5,
    sunday    = 6
)

inline DayOfWeek dayOfWeek(Qt::DayOfWeek day)
{
    return DayOfWeek(day - 1);
}

} // namespace nx::vms::api
