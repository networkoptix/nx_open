// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::rules
{

class EventFilter;
class ActionBuilder;

} // namespace nx::vms::rules

namespace nx::vms::client::desktop {
namespace vms_rules {

/**
 * Returns human readable representation of event type.
 */
QString getDisplayName(const nx::vms::rules::EventFilter* event);

/**
 * Returns human readable representation of action type.
 */
QString getDisplayName(const nx::vms::rules::ActionBuilder* action);

} // namespace vms_rules
} // namespace nx::vms::client::desktop
