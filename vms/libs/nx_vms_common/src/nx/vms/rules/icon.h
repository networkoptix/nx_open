// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::rules {

NX_REFLECTION_ENUM_CLASS(Icon,
    calculated, //< Icon is calculated from event level. Default value.
    none,
    alert,
    important,
    critical,
    server,
    camera,
    motion,
    resource,
    custom);

} // namespace nx::vms::api::rules

Q_DECLARE_METATYPE(nx::vms::rules::Icon)
