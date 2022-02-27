// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>

#include <Qt> //< For enum Qt::DayOfWeek.

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(DayOfWeek,
    none = 0,
    monday = 1 << 0,
    tuesday = 1 << 1,
    wednesday = 1 << 2,
    thursday = 1 << 3,
    friday = 1 << 4,
    saturday = 1 << 5,
    sunday = 1 << 6
)

inline DayOfWeek dayOfWeek(Qt::DayOfWeek day)
{
    static_assert(Qt::Monday == 1 && Qt::Sunday == 7); //< Qt week starts on Monday and is 1-based.
    return DayOfWeek(1 << (day - Qt::Monday));
}

} // namespace nx::vms::api
