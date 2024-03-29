// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(ConnectionType,
    insecure = 0,
    ssl = 1,
    tls = 2
)

} // namespace nx::vms::api
