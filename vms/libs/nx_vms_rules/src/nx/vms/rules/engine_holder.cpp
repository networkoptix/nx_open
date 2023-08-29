// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine_holder.h"

#include <api/common_message_processor.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_vms_rules_manager.h>

#include "ec2_router.h"
#include "plugin.h"
#include "rule.h"

namespace nx::vms::rules {

EngineHolder::EngineHolder(
    nx::vms::common::SystemContext* context,
    std::unique_ptr<Plugin> plugin,
    bool separateThread)
    :
    m_builtinPlugin(std::move(plugin)),
    m_engine(std::make_unique<Engine>(std::make_unique<Ec2Router>(context)))
{
    m_builtinPlugin->initialize(m_engine.get());

    if (separateThread)
    {
        m_thread = std::make_unique<QThread>();
        m_thread->setObjectName("VmsRulesEngine");
        m_thread->start();

        m_engine->moveToThread(m_thread.get());
    }

    context->setVmsRulesEngine(m_engine.get());
}

EngineHolder::~EngineHolder()
{
    stop();
}

void EngineHolder::stop()
{
    if (!m_thread)
        return;

    nx::utils::AsyncHandlerExecutor(m_thread.get()).submit(
        [this]
        {
            m_engine.reset();
            m_thread->quit();
        });

    m_thread->wait();
    m_thread.reset();
}

Engine* EngineHolder::engine() const
{
    return m_engine.get();
}

void EngineHolder::connectEngine(
    Engine* engine,
    const QnCommonMessageProcessor* processor,
    Qt::ConnectionType connectionType)
{
    if (!NX_ASSERT(processor))
        return;

    connect(processor, &QnCommonMessageProcessor::vmsRulesReset, engine,
        [engine](QnUuid peerId, const nx::vms::api::rules::RuleList& rules)
        {
            engine->setId(peerId);
            engine->resetRules(rules);
        },
        connectionType);

    connect(processor, &QnCommonMessageProcessor::vmsRuleUpdated, engine,
        [engine](const nx::vms::api::rules::Rule& ruleData, ec2::NotificationSource /*source*/)
        {
            engine->updateRule(ruleData);
        },
        connectionType);

    connect(processor, &QnCommonMessageProcessor::vmsRuleRemoved, engine,
        [engine](QnUuid id)
        {
            engine->removeRule(id);
        },
        connectionType);
}

} // namespace nx::vms::rules
