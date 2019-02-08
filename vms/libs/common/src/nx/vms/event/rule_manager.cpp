#include "rule_manager.h"

#include <nx/vms/event/rule.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace vms {
namespace event {

RuleManager::RuleManager(QObject* parent):
    base_type(parent)
{
}

RuleManager::~RuleManager()
{
}

RuleList RuleManager::rules() const
{
    QnMutexLocker lock(&m_mutex);
    return m_rules.values();
}

RulePtr RuleManager::rule(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    return m_rules[id];
}

void RuleManager::resetRules(const RuleList& rules)
{
    QnMutexLocker lock(&m_mutex);

    m_rules.clear();
    for (const auto& rule: rules)
        m_rules[rule->id()] = rule;

    lock.unlock();

    emit rulesReset(rules);
}

void RuleManager::addOrUpdateRule(const RulePtr& rule)
{
    NX_ASSERT(rule);
    if (!rule)
        return;

    QnMutexLocker lock(&m_mutex);
    const bool added = !m_rules.contains(rule->id());
    m_rules[rule->id()] = rule;
    lock.unlock();

    emit ruleAddedOrUpdated(rule, added);
}

void RuleManager::removeRule(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
    const auto iter = m_rules.find(id);
    if (iter == m_rules.end())
        return;

    m_rules.erase(iter);
    lock.unlock();

    emit ruleRemoved(id);
}

} // namespace event
} // namespace vms
} // namespace nx
