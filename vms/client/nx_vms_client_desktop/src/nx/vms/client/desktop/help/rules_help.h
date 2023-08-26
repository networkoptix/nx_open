// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_health/message_type.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::client::desktop {

namespace rules {

int eventHelpId(nx::vms::api::EventType type);
int actionHelpId(nx::vms::api::ActionType type);
int healthHelpId(nx::vms::common::system_health::MessageType type);

} // namespace rules

} // namespace nx::vms::client::desktop
