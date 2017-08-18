#include "rule_holder.h"

#include <vector>
#include <algorithm>

#include <nx/vms/event/rule.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::vms;

std::set<QnUuid> RuleHolder::addRule(const nx::vms::event::RulePtr& rule)
{
    QnMutexLocker lock(&m_mutex);
    return addRuleUnsafe(rule);
}

std::set<QnUuid> RuleHolder::updateRule(const nx::vms::event::RulePtr& rule)
{
    QnMutexLocker lock(&m_mutex);
    std::set<QnUuid> result;
    auto ruleId = rule->id();
    bool watched = isRuleBeingWatched(ruleId);
    bool analyticsSdkEventRule = isAnalyticsSdkEventRule(rule);
    bool disabled = rule->isDisabled();

    if (!watched && !analyticsSdkEventRule)
        return result;

    if (watched && (!analyticsSdkEventRule || disabled))
        return removeRuleUnsafe(ruleId);

    if (!watched && analyticsSdkEventRule && !disabled)
        return addRuleUnsafe(rule);

    auto eventTypeId = rule->eventParams().analyticsEventId();
    bool eventIsTheSame = m_ruleEventMap[ruleId] == eventTypeId;

    // Change rule event id
    m_ruleEventMap[ruleId] == eventTypeId;

    auto& currentRuleResources = m_ruleResourceMap[ruleId];
    auto newRuleResources = eventResources(rule->eventResources());

    std::set<QnUuid> toRemove;
    std::set<QnUuid> toAdd;
    std::set<QnUuid> affectedResources;

    std::set_difference(
        currentRuleResources.cbegin(), currentRuleResources.cend(),
        newRuleResources.cbegin(), newRuleResources.cend(),
        std::inserter(toRemove, toRemove.end()));

    std::set_difference(
        newRuleResources.cbegin(), newRuleResources.cend(),
        currentRuleResources.cbegin(), currentRuleResources.cend(),
        std::inserter(toAdd, toAdd.end()));

    // Remove rule from resources
    for (const auto& res: toRemove)
    {
        m_resourceRuleMap[res].erase(ruleId);
        if (eventIsTheSame)
            affectedResources.insert(res);
    }
    
    // Add rule to resources
    for (const auto& res: toAdd)
    {
        m_resourceRuleMap[res].insert(ruleId);
        if (eventIsTheSame)
            affectedResources.insert(res);
    }
        
    if (!eventIsTheSame)
    {
        std::set_union(
            currentRuleResources.cbegin(), currentRuleResources.cend(),
            newRuleResources.cbegin(), newRuleResources.cend(),
            std::inserter(affectedResources, affectedResources.end()));
    }

    // Change rule resources
    m_ruleResourceMap[ruleId] = std::move(newRuleResources);

    for (const auto& res: affectedResources)
    {
        m_resourceEventMap[res] = calculateResourceEvents(res);
        result.insert(res);
    }

    return result;
}

std::set<QnUuid> RuleHolder::removeRule(const QnUuid& ruleId)
{
    QnMutexLocker lock(&m_mutex);
    return removeRuleUnsafe(ruleId);
}

std::set<QnUuid> RuleHolder::resetRules(const nx::vms::event::RuleList& rules)
{
    std::set<QnUuid> result;
    std::set<QnUuid> resourcesWithNoRules;

    for (const auto& item: m_resourceEventMap)
        resourcesWithNoRules.insert(item.first);

    m_resourceRuleMap.clear();
    m_ruleResourceMap.clear();
    m_resourceEventMap.clear();
    m_ruleEventMap.clear();
    
    for (const auto& rule: rules)
    {
        if (!isAnalyticsSdkEventRule(rule))
            continue;

        auto ruleId = rule->id();
        auto ruleResources = eventResources(rule->eventResources());

        m_ruleResourceMap.emplace(ruleId, ruleResources);

        for (const auto& res : ruleResources)
        {
            auto& resourceEvents = m_resourceEventMap[res];
            auto eventTypeId = rule->eventParams().analyticsEventId();

            m_resourceRuleMap[res].insert(ruleId);
            m_ruleEventMap[ruleId] = eventTypeId;
            resourceEvents.insert(eventTypeId);
        }
    }

    for (const auto& item: m_resourceRuleMap)
    {
        const auto& res = item.first; 
        result.insert(res);
        resourcesWithNoRules.erase(res);
    }

    for(const auto& res: resourcesWithNoRules)
        result.insert(res);

    return result;
}

std::set<QnUuid> RuleHolder::watchedEvents(const QnUuid& resourceId) const
{
    QnMutexLocker lock(&m_mutex);
    auto events = m_resourceEventMap.find(resourceId);

    if (events != m_resourceEventMap.cend())
        return events->second;

    return std::set<QnUuid>();
}

std::set<QnUuid> RuleHolder::addRuleUnsafe(const nx::vms::event::RulePtr& rule)
{
    std::set<QnUuid> result;
    const bool disabled = rule->isDisabled();

    if (!isAnalyticsSdkEventRule(rule) || disabled)
        return result;

    auto ruleId = rule->id();
    auto ruleResources = eventResources(rule->eventResources());

    m_ruleResourceMap.emplace(ruleId, ruleResources);

    for (const auto& res : ruleResources)
    {
        auto& resourceEvents = m_resourceEventMap[res];
        auto eventTypeId = rule->eventParams().analyticsEventId();

        m_resourceRuleMap[res].insert(ruleId);
        m_ruleEventMap[ruleId] = eventTypeId;
        auto insertion = resourceEvents.insert(eventTypeId);

        if (insertion.second)
            result.insert(res);
    }

    return result;
}

std::set<QnUuid> RuleHolder::removeRuleUnsafe(const QnUuid& ruleId)
{
    std::set<QnUuid> result;
    if (!isRuleBeingWatched(ruleId))
        return result;

    for (const auto& res : m_ruleResourceMap[ruleId])
    {
        m_resourceRuleMap[res].erase(ruleId);
        m_resourceEventMap[res].erase(m_ruleEventMap[ruleId]);

        result.insert(res);
    }

    m_ruleResourceMap.erase(ruleId);
    m_ruleEventMap.erase(ruleId);

    return result;
}

std::set<QnUuid> RuleHolder::eventResources(const QVector<QnUuid>& resources) const
{
    std::set<QnUuid> result;
    for (const auto& res: resources)
        result.insert(res);

    return result;
}

bool RuleHolder::isRuleBeingWatched(const QnUuid& ruleId) const
{
    return m_ruleResourceMap.find(ruleId) != m_ruleResourceMap.cend();
}

bool RuleHolder::isAnalyticsSdkEventRule(const event::RulePtr& rule) const
{
    return rule->eventType() == event::EventType::analyticsSdkEvent;
}

std::set<QnUuid> RuleHolder::calculateResourceEvents(const QnUuid& resourceId) const
{
    std::set<QnUuid> events;

    const auto resourceRulesItr = m_resourceRuleMap.find(resourceId);
    if (resourceRulesItr == m_resourceRuleMap.cend())
        return events;

    for (const auto& ruleId: resourceRulesItr->second)
        events.insert(m_ruleEventMap.find(ruleId)->second);

    return events;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx