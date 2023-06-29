// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/rules/rules_fwd.h>

namespace nx::vms::client::desktop {

/** Helper class to sort rules types lexicographically. */
class VmsRulesTypeComparator: public SystemContextAware
{
public:
    explicit VmsRulesTypeComparator(SystemContext* context);

    bool lexEventLess(const QString& left, const QString& right) const;
    bool lexActionLess(const QString& left, const QString& right) const;

    /*
     * Reorder event types to lexicographical order (for sorting)
     */
    int toLexEventType(const QString& eventType) const;

    /*
     * Reorder actions types to lexicographical order (for sorting)
     */
    int toLexActionType(const QString& actionType) const;

    QStringList lexSortedEvents(const QStringList& events) const;

    QStringList lexSortedActions(const QStringList& actions) const;

private:
    void initLexOrdering();

private:
    QMap<QString, int> m_eventTypeToLexOrder;
    QMap<QString, int> m_actionTypeToLexOrder;
};

} // namespace nx::vms::client::desktop
