#pragma once

#include <nx/vms/event/event_fwd.h>
#include <health/system_health.h>

// TODO: #vkutin Think of a proper namespace
namespace QnBusiness {

int eventHelpId(nx::vms::event::EventType type);
int actionHelpId(nx::vms::event::ActionType type);
int healthHelpId(QnSystemHealth::MessageType type);

} // namespace QnBusiness
