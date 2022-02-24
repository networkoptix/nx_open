// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/event_fwd.h>
#include <health/system_health.h>

// TODO: #vkutin Think of a proper namespace
namespace QnBusiness {

int eventHelpId(nx::vms::api::EventType type);
int actionHelpId(nx::vms::api::ActionType type);
int healthHelpId(QnSystemHealth::MessageType type);

} // namespace QnBusiness
