// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_router.h"

#include <api/common_message_processor.h>
#include <api/server_rest_connection.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/event_info.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/serialization.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_vms_rules_manager.h>
#include <transaction/message_bus_adapter.h>
#include <transaction/transaction.h>

namespace nx::vms::client::core::rules {

using namespace nx::vms::rules;

ClientRouter::ClientRouter(nx::vms::client::core::SystemContext* context):
    nx::vms::client::core::SystemContextAware(context)
{
}

ClientRouter::~ClientRouter()
{
}

nx::Uuid ClientRouter::peerId() const
{
    return SystemContextAware::peerId();
}

void ClientRouter::routeEvent(
    const EventPtr& event,
    const RuleList& triggeredRules)
{
    NX_ASSERT(false, "Currently we don't send Events from the Client: %1", event->type());
}

void ClientRouter::routeAction(const ActionPtr& action)
{
    NX_ASSERT(false, "Currently we don't send Actions from the Client: %1", action->type());
}

} // namespace nx::vms::client::core::rules
