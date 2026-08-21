// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <unordered_map>
#include <vector>

#include <QtCore/QString>

#include <nx/utils/uuid.h>

#include "rules_fwd.h"

namespace nx::vms::rules {

/**
 * Caches the rule properties which are immutable during the rule lifetime, so the engine does not
 * have to calculate them for the every incoming event. Additionally indexes the rules by their
 * event types, so only the rules which are able to match the event are considered.
 *
 * The class is not thread safe. All the methods must be called with the engine rule mutex locked.
 */
class NX_VMS_RULES_API RuleCache
{
public:
    struct Entry
    {
        ConstRulePtr rule;

        /** Rule event filters of the event type the entry is indexed by only. */
        std::vector<const EventFilter*> filters;
    };

    /** Adds the given rule to the cache or replaces the existing one. */
    void update(const ConstRulePtr& rule);

    /** Removes the rule with the given id from the cache, does nothing if it is not cached. */
    void remove(nx::Uuid ruleId);

    /** Drops all the cached data and calculates it again for the given rule set. */
    void reset(const std::unordered_map<nx::Uuid, RulePtr>& rules);

    /**
     * Recalculates compatibility of the rules whose compatibility depends on the analytics
     * taxonomy state and reindexes the ones the flag has been changed for.
     */
    void onTaxonomyChanged();

    bool isCompatible(nx::Uuid ruleId) const;

    /**
     * Enabled and compatible rules having at least one event filter of the given type. Returns an
     * empty list if there are no such rules.
     */
    const std::vector<Entry>& byEventType(const QString& eventType) const;

private:
    struct RuleInfo
    {
        ConstRulePtr rule;

        bool enabled = false;
        bool isCompatible = false;
        bool taxonomyDependent = false;

        std::set<QString> eventTypes;
    };

    /** Adds the rule to the index if it is able to produce actions, does nothing otherwise. */
    void addToIndex(const RuleInfo& info);
    void removeFromIndex(const RuleInfo& info);

private:
    std::unordered_map<nx::Uuid, RuleInfo> m_ruleInfo;
    std::unordered_map<QString, std::vector<Entry>> m_byEventType;
};

} // namespace nx::vms::rules
