// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(TimePeriodContentType,
    recording,
    motion,
    analytics
)

} // namespace nx::vms::api
