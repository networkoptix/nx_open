// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::rules {

NX_REFLECTION_ENUM_CLASS(Icon,
    alert, //< Alert icon color is calculated from the event level. Default value.
    none,
    server,
    storage,
    networkIssue,
    license,
    motion,
    resource,
    inputSignal,
    generic,
    pluginDiagnostic,
    fanError,
    cloudOffline,
    analyticsEvent,
    analyticsObjectDetected,
    softTrigger)

} // namespace nx::vms::rules
