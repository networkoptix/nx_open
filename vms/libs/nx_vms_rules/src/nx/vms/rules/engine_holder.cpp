// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine_holder.h"

#include <api/common_message_processor.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_vms_rules_manager.h>
#include <nx/vms/common/system_context.h>

#include "ec2_router.h"
#include "plugin.h"
#include "rule.h"

namespace nx::vms::rules {

EngineHolder::EngineHolder(nx::vms::common::SystemContext* context, std::unique_ptr<Plugin> plugin):
    m_engine(std::make_unique<Engine>(std::make_unique<Ec2Router>(context))),
    m_builtinPlugin(std::move(plugin))
{
    m_builtinPlugin->initialize(m_engine.get());
}

EngineHolder::~EngineHolder()
{
}

Engine* EngineHolder::engine() const
{
    return m_engine.get();
}

void EngineHolder::connectToConnection(
    Engine* engine,
    nx::vms::common::SystemContext* context,
    Qt::ConnectionType connectionType)
{
    const auto vmsRulesManager = context->ec2Connection()->vmsRulesNotificationManager();

    connect(vmsRulesManager.get(), &ec2::AbstractVmsRulesNotificationManager::ruleUpdated, engine,
        [engine](const nx::vms::api::rules::Rule& ruleData, ec2::NotificationSource source)
        {
            if (auto rule = engine->buildRule(ruleData))
                engine->updateRule(std::move(rule));
        },
        connectionType);

    connect(vmsRulesManager.get(), &ec2::AbstractVmsRulesNotificationManager::ruleRemoved, engine,
        [engine](QnUuid id)
        {
            engine->removeRule(id);
        },
        connectionType);

    connect(context->messageProcessor(), &QnCommonMessageProcessor::vmsRulesReset, engine,
        [engine](QnUuid peerId, const nx::vms::api::rules::RuleList& rules)
        {
            engine->init(peerId, rules);
        },
        connectionType);
}

} // namespace nx::vms::rules
