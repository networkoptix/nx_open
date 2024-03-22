// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ec2_router.h"

#include <api/common_message_processor.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_vms_rules_manager.h>

#include "ini.h"

namespace {

const auto kFakeHandler = [](int /*handle*/, ec2::ErrorCode errorCode)
{
    if (errorCode != ec2::ErrorCode::ok)
        NX_ERROR(NX_SCOPE_TAG, "Event routing error: %1", toString(errorCode));
};

}

namespace nx::vms::rules {

Ec2Router::Ec2Router(nx::vms::common::SystemContext* context):
    nx::vms::common::SystemContextAware(context)
{
}

Ec2Router::~Ec2Router()
{
}

void Ec2Router::init(const nx::Uuid& id)
{
    m_id = id;

    if (!m_connected)
    {
        connect(
            m_context->messageProcessor(),
            &QnCommonMessageProcessor::vmsEventReceived,
            this,
            &Ec2Router::onEventReceived);
        m_connected = true;
    }
}

void Ec2Router::routeEvent(
        const EventData& eventData,
        const QSet<nx::Uuid>& triggeredRules,
        const QSet<nx::Uuid>& affectedResources)
{
    QSet<nx::Uuid> peers;
    for (const auto& id: affectedResources)
    {
        const auto& ptr = resourcePool()->getResourceById(id);
        if (!ptr)
            continue;

        const auto& server = ptr->getParentId();
        if (server.isNull())
            continue; //< TODO: #spanasenko Check all resource types...

        peers += server;
    }
    //TODO: #spanasenko Add client peers.

    nx::vms::api::rules::EventInfo info;
    info.props = eventData;

    for (const auto ruleId: triggeredRules)
    {
        info.ruleId = ruleId;

        if (nx::vms::rules::ini().fullSupport)
        {
            messageBusConnection()->getVmsRulesManager(nx::network::rest::kSystemSession)
                ->broadcastEvent(info, kFakeHandler);
        }
        else // Execute locally.
        {
            onEventReceived(info);
        }
    }
}

void Ec2Router::onEventReceived(const nx::vms::api::rules::EventInfo& eventInfo)
{
    emit eventReceived(eventInfo.ruleId, eventInfo.props);
}

} // namespace nx::vms::rules
