// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rule_cache.h"

#include <algorithm>

#include <nx/utils/log/assert.h>

#include "event_filter.h"
#include "events/analytics_event.h"
#include "rule.h"
#include "utils/type.h"

namespace nx::vms::rules {

namespace {

// TODO: #amalov Derive the list from the event manifests.
bool isTaxonomyDependentEventType(const QString& eventType)
{
    static const auto kAnalyticsEventType = utils::type<AnalyticsEvent>();

    return eventType == kAnalyticsEventType;
}

} // namespace

void RuleCache::update(const ConstRulePtr& rule)
{
    const auto ruleId = rule->id();

    remove(ruleId);

    RuleInfo info{.rule = rule};
    info.enabled = info.rule->enabled();
    info.isCompatible = info.rule->isCompatible();

    const auto filters = info.rule->eventFilters();

    for (const auto filter: filters)
    {
        auto type = filter->eventType();
        info.taxonomyDependent |= isTaxonomyDependentEventType(type);
        info.eventTypes.insert(std::move(type));
    }

    const auto it = m_ruleInfo.insert_or_assign(ruleId, std::move(info)).first;
    addToIndex(it->second);
}

void RuleCache::remove(nx::Uuid ruleId)
{
    const auto it = m_ruleInfo.find(ruleId);
    if (it == m_ruleInfo.end())
        return;

    removeFromIndex(it->second);
    m_ruleInfo.erase(it);
}

void RuleCache::reset(const std::unordered_map<nx::Uuid, RulePtr>& rules)
{
    m_ruleInfo.clear();
    m_byEventType.clear();

    for (const auto& [_, rule]: rules)
        update(rule);
}

void RuleCache::onTaxonomyChanged()
{
    for (auto& [_, info]: m_ruleInfo)
    {
        if (!info.taxonomyDependent)
            continue;

        const auto isCompatible = info.rule->isCompatible();
        if (isCompatible == info.isCompatible)
            continue;

        removeFromIndex(info);
        info.isCompatible = isCompatible;
        addToIndex(info);
    }
}

bool RuleCache::isCompatible(nx::Uuid ruleId) const
{
    const auto it = m_ruleInfo.find(ruleId);

    return it != m_ruleInfo.end() && it->second.isCompatible;
}

const std::vector<RuleCache::Entry>& RuleCache::byEventType(const QString& eventType) const
{
    static const std::vector<Entry> kEmptyEntryList;

    const auto it = m_byEventType.find(eventType);

    return it == m_byEventType.end() ? kEmptyEntryList : it->second;
}

void RuleCache::addToIndex(const RuleInfo& info)
{
    if (!info.enabled || !info.isCompatible)
        return;

    const auto filters = info.rule->eventFilters();

    for (const auto& eventType: info.eventTypes)
    {
        Entry entry{.rule = info.rule};

        for (const auto filter: filters)
        {
            if (filter->eventType() == eventType)
                entry.filters.push_back(filter);
        }

        m_byEventType[eventType].push_back(std::move(entry));
    }
}

void RuleCache::removeFromIndex(const RuleInfo& info)
{
    const auto ruleId = info.rule->id();

    for (const auto& eventType: info.eventTypes)
    {
        const auto it = m_byEventType.find(eventType);
        if (it == m_byEventType.end())
            continue;

        std::erase_if(
            it->second, [&ruleId](const Entry& entry) { return entry.rule->id() == ruleId; });

        if (it->second.empty())
            m_byEventType.erase(it);
    }
}

} // namespace nx::vms::rules
