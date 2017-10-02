#include "event_rule_watcher.h"

#include <nx/vms/event/rule.h>
#include <media_server/media_server_module.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::vms;

EventRuleWatcher::EventRuleWatcher(QObject* parent):
    Connective<QObject>(parent),
    m_ruleHolder(qnServerModule->commonModule())
{
    qRegisterMetaType<std::set<QnUuid>>();

    auto ruleManager = qnServerModule
        ->commonModule()
        ->eventRuleManager();

    connect(
        ruleManager, &event::RuleManager::rulesReset,
        this, &EventRuleWatcher::at_rulesReset);

    connect(
        ruleManager, &event::RuleManager::ruleAddedOrUpdated,
        this, &EventRuleWatcher::at_ruleAddedOrUpdated);

    connect(
        ruleManager, &event::RuleManager::ruleRemoved,
        this, &EventRuleWatcher::at_ruleRemoved);
}

EventRuleWatcher::~EventRuleWatcher()
{
}

void EventRuleWatcher::at_rulesReset(const event::RuleList& rules)
{
    emit rulesUpdated(m_ruleHolder.resetRules(rules));
}

void EventRuleWatcher::at_ruleAddedOrUpdated(const event::RulePtr& rule, bool added)
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

std::set<QnUuid> EventRuleWatcher::watchedEventsForResource(const QnUuid& resourceId)
{
    return m_ruleHolder.watchedEvents(resourceId);
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
