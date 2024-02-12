// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_rules.values();
}

RulePtr RuleManager::rule(const nx::Uuid& id) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_rules[id];
}

void RuleManager::resetRules(const RuleList& rules)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

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

    NX_MUTEX_LOCKER lock(&m_mutex);
    const bool added = !m_rules.contains(rule->id());
    m_rules[rule->id()] = rule;
    lock.unlock();

    emit ruleAddedOrUpdated(rule, added);
}

void RuleManager::removeRule(const nx::Uuid& id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
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
