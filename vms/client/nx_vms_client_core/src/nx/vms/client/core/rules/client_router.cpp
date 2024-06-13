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
#include <nx/vms/rules/engine.h>
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

void ClientRouter::init(const QnCommonMessageProcessor* processor)
{
    connect(processor, &QnCommonMessageProcessor::vmsActionReceived,
        this, &ClientRouter::onActionReceived,
        Qt::DirectConnection);
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

void ClientRouter::routeAction(const ActionPtr& action, Flags /*flags*/)
{
    NX_ASSERT(false, "Currently we don't send Actions from the Client: %1", action->type());
}

void ClientRouter::onActionReceived(const nx::vms::api::rules::ActionInfo& info)
{
    const auto engine = systemContext()->vmsRulesEngine();

    const auto type = info.props.value("type").toString();
    const auto manifest = engine->actionDescriptor(type);

    if (!manifest)
    {
        NX_WARNING(this, "Unknown action received: %1", type);
        return;
    }

    if (!manifest->executionTargets.testFlag(ExecutionTarget::clients))
    {
        NX_WARNING(this, "Unexpected action received: %1", type);
        return;
    }

    const auto action = engine->buildAction(info.props);
    if (!action)
    {
        NX_WARNING(this, "Invalid action data: %1", type);
        return;
    }

    receiveAction(action);
}

} // namespace nx::vms::client::core::rules
