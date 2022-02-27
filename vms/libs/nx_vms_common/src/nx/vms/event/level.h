// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "event_fwd.h"

namespace nx::vms::event {

/**
 * Importance level of a notification.
 */
enum class Level
{
    no,
    common,
    other,
    success,
    important,
    critical,
    count
};

NX_VMS_COMMON_API Level levelOf(const AbstractActionPtr& action);
NX_VMS_COMMON_API Level levelOf(const EventParameters& params);

} // namespace nx::vms::event
