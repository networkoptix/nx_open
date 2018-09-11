#include "event_rule_watcher.h"

#include <nx/vms/event/rule.h>
#include <nx/mediaserver/metadata/event_type_mapping.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/event/rule_manager.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::vms::event;

EventRuleWatcher::EventRuleWatcher(QnMediaServerModule* serverModule):
    Connective<QObject>(serverModule),
    ServerModuleAware(serverModule),
    m_ruleHolder(serverModule->commonModule())
{
    auto ruleManager = serverModule->commonModule()->eventRuleManager();

    auto resourcePool = serverModule
        ->commonModule()
        ->resourcePool();

    connect(
        ruleManager, &RuleManager::rulesReset,
        this, &EventRuleWatcher::at_rulesReset);

    connect(
        ruleManager, &RuleManager::ruleAddedOrUpdated,
        this, &EventRuleWatcher::at_ruleAddedOrUpdated);

    connect(
        ruleManager, &RuleManager::ruleRemoved,
        this, &EventRuleWatcher::at_ruleRemoved);

    connect(
        resourcePool, &QnResourcePool::resourceAdded,
        this, &EventRuleWatcher::at_resourceAdded);
}

EventRuleWatcher::~EventRuleWatcher()
{
}

void EventRuleWatcher::at_rulesReset(const RuleList& rules)
{
    emit rulesUpdated(m_ruleHolder.resetRules(rules));
}

void EventRuleWatcher::at_ruleAddedOrUpdated(const RulePtr& rule, bool added)
{
    if (added)
        emit rulesUpdated(m_ruleHolder.addRule(rule));
    else
        emit rulesUpdated(m_ruleHolder.updateRule(rule));
}

void EventRuleWatcher::at_ruleRemoved(const QnUuid& ruleId)
{
    emit rulesUpdated(m_ruleHolder.removeRule(ruleId));
}

void EventRuleWatcher::at_resourceAdded(const QnResourcePtr& resource)
{
    emit rulesUpdated(m_ruleHolder.addResource(resource));
}

QSet<QnUuid> EventRuleWatcher::watchedEventsForResource(const QnUuid& resourceId)
{
    return m_ruleHolder.watchedEvents(resourceId);
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
