#pragma once

#include <nx/vms/event/event_fwd.h>

#include <client_core/connection_context_aware.h>

// TODO: #vkutin Get rid of 'business' and put this class to proper namespace.

/** Helper class to sort business types lexicographically. */
class QnBusinessTypesComparator: public QObject, public QnConnectionContextAware
{
    using base_type = QObject;
public:
    enum class EventSubType
    {
        user,
        failure,
        success,
        any
    };

    explicit QnBusinessTypesComparator(bool onlyUserAvailableActions, QObject *parent = nullptr);

    bool lexicographicalLessThan(nx::vms::event::EventType left, nx::vms::event::EventType right) const;
    bool lexicographicalLessThan(nx::vms::event::ActionType left, nx::vms::event::ActionType right) const;

    /*
     * Reorder event types to lexicographical order (for sorting)
     */
    int toLexEventType(nx::vms::event::EventType eventType) const;

    /*
     * Reorder actions types to lexicographical order (for sorting)
     */
    int toLexActionType(nx::vms::event::ActionType actionType) const;

    QList<nx::vms::event::EventType> lexSortedEvents(EventSubType subtype = EventSubType::any) const;
    QList<nx::vms::event::ActionType> lexSortedActions() const;

private:
    void initLexOrdering();

    QList<nx::vms::event::ActionType> getAllActions() const;

private:
    QVector<int> m_eventTypeToLexOrder;
    QVector<int> m_actionTypeToLexOrder;
    const bool m_onlyUserAvailableActions;
};
