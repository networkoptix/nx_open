// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

// Please note: enum is numeric-serialized, reordering is forbidden.
enum class RadassMode
{
    Auto = 0,
    High = 1,
    Low = 2,
    Custom = 3,
};

NX_REFLECTION_INSTRUMENT_ENUM(RadassMode,
    Auto,
    High,
    Low)

using RadassModeByLayoutItemIdHash = QHash<nx::Uuid, RadassMode>;

} // namespace nx::vms::client::desktop
