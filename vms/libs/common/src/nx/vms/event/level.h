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

Level levelOf(const AbstractActionPtr& action);
Level levelOf(const EventParameters& params);

} // namespace nx::vms::event
