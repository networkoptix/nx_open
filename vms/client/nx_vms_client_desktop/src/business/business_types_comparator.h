#pragma once

#include <nx/vms/event/event_fwd.h>

#include <client_core/connection_context_aware.h>

// TODO: #vkutin Get rid of 'business' and put this class to proper namespace.

/** Helper class to sort business types lexicographically. */
class QnBusinessTypesComparator: public QObject, public QnConnectionContextAware
{
    using base_type = QObject;
public:
    explicit QnBusinessTypesComparator(QObject* parent = nullptr);

    bool lexicographicalLessThan(nx::vms::api::EventType left, nx::vms::api::EventType right) const;
    bool lexicographicalLessThan(nx::vms::api::ActionType left, nx::vms::api::ActionType right) const;

    /*
     * Reorder event types to lexicographical order (for sorting)
     */
    int toLexEventType(nx::vms::api::EventType eventType) const;

    /*
     * Reorder actions types to lexicographical order (for sorting)
     */
    int toLexActionType(nx::vms::api::ActionType actionType) const;

    QList<nx::vms::api::EventType> lexSortedEvents(
        const QList<nx::vms::api::EventType>& events) const;

    QList<nx::vms::api::ActionType> lexSortedActions(
        const QList<nx::vms::api::ActionType>& actions) const;

private:
    void initLexOrdering();

private:
    QVector<int> m_eventTypeToLexOrder;
    QVector<int> m_actionTypeToLexOrder;
};
