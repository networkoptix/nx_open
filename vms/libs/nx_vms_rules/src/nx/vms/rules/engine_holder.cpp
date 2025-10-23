// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine_holder.h"

#include <api/common_message_processor.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_vms_rules_manager.h>

#include "plugin.h"
#include "router.h"
#include "rule.h"

namespace nx::vms::rules {

EngineHolder::EngineHolder(
    nx::vms::common::SystemContext* context,
    std::unique_ptr<Router> router,
    std::unique_ptr<Plugin> plugin,
    bool separateThread)
    :
    m_builtinPlugin(std::move(plugin)),
    m_engine(std::make_unique<Engine>(context, std::move(router)))
{
    m_builtinPlugin->initialize(m_engine.get());

    if (separateThread)
    {
        m_thread = std::make_unique<QThread>();
        m_thread->setObjectName("VmsRulesEngine");
        m_thread->start();

        m_engine->moveToThread(m_thread.get());
    }
}

EngineHolder::~EngineHolder()
{
    stop();
}

void EngineHolder::stop()
{
    if (!m_thread)
        return;

    NX_DEBUG(this, "Stopping Engine thread");
    nx::utils::AsyncHandlerExecutor(m_thread.get()).submit(
        [this]
        {
            m_engine.reset();
            m_thread->quit();
        });

    m_thread->wait();
    m_thread.reset();
    NX_DEBUG(this, "Engine thread stopped");
}

Engine* EngineHolder::engine() const
{
    return m_engine.get();
}

void EngineHolder::connectEngine(
    const QnCommonMessageProcessor* processor,
    Qt::ConnectionType connectionType)
{
    if (!NX_ASSERT(processor))
        return;

    const auto engine = m_engine.get();
    connect(processor, &QnCommonMessageProcessor::vmsRulesReset, engine,
        [this, engine](nx::Uuid /*peerId*/, const nx::vms::api::rules::RuleList& rules)
        {
            this->reinitializeDynamicPlugins();
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
        [engine](nx::Uuid id)
        {
            engine->removeRule(id);
        },
        connectionType);

    engine->router()->init(processor);
}

void EngineHolder::addDynamicPlugin(std::unique_ptr<Plugin> plugin)
{
    auto addPlugin =
        [this, plugin = std::move(plugin)]() mutable
        {
            m_dynamicPlugins.push_back(std::move(plugin));
        };
    const auto thread = m_thread.get();
    if (thread)
        nx::utils::AsyncHandlerExecutor(thread).submit(std::move(addPlugin));
    else
        addPlugin();

}

// Must be called in the Engine thread.
void EngineHolder::reinitializeDynamicPlugins()
{
    NX_ASSERT(m_engine, "Engine is not initialized");

    for (auto& plugin: m_dynamicPlugins)
    {
        plugin->deinitialize();
        plugin->initialize(m_engine.get());
    }
}

} // namespace nx::vms::rules
