#include "event_rule_manager.h"

#include <business/business_event_rule.h>
#include <nx/utils/log/assert.h>

QnEventRuleManager::QnEventRuleManager(QObject* parent):
    base_type(parent)
{
}

QnEventRuleManager::~QnEventRuleManager()
{
}

QnBusinessEventRuleList QnEventRuleManager::rules() const
{
    QnMutexLocker lock(&m_mutex);
    return m_rules.values();
}

QnBusinessEventRulePtr QnEventRuleManager::rule(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    return m_rules[id];
}

void QnEventRuleManager::resetRules(const QnBusinessEventRuleList& rules)
{
    QnMutexLocker lock(&m_mutex);

    m_rules.clear();
    for (const auto& rule: rules)
        m_rules[rule->id()] = rule;

    lock.unlock();

    emit rulesReset(rules);
}

void QnEventRuleManager::addOrUpdateRule(const QnBusinessEventRulePtr& rule)
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

void QnEventRuleManager::removeRule(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
    const auto iter = m_rules.find(id);
    if (iter == m_rules.end())
        return;

    m_rules.erase(iter);
    lock.unlock();

    emit ruleRemoved(id);
}
