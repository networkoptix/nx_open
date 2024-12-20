// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFlags>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/json/flags.h>

namespace nx::vms::common {
namespace ptz {

NX_REFLECTION_ENUM_CLASS(DataField,
    none = 0,
    capabilities = 1 << 0,
    devicePosition = 1 << 1,
    logicalPosition = 1 << 2,
    deviceLimits = 1 << 3,
    logicalLimits = 1 << 4,
    flip = 1 << 5,
    presets = 1 << 6,
    tours = 1 << 7,
    activeObject = 1 << 8,
    homeObject = 1 << 9,
    auxiliaryTraits = 1 << 10,

    all = (1 << 11) - 1
)

Q_DECLARE_FLAGS(DataFields, DataField)
Q_DECLARE_OPERATORS_FOR_FLAGS(DataFields)

} // namespace ptz
} // namespace nx::vms::common
