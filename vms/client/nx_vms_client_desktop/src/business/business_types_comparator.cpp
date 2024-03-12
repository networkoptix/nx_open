// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "business_types_comparator.h"

#include <nx/vms/event/strings_helper.h>

using namespace nx;
using namespace vms::event;

QnBusinessTypesComparator::QnBusinessTypesComparator(QObject* parent):
    base_type(parent)
{
    initLexOrdering();
}

bool QnBusinessTypesComparator::lexicographicalLessThan(EventType left, EventType right) const
{
    return toLexEventType(left) < toLexEventType(right);
}

bool QnBusinessTypesComparator::lexicographicalLessThan(ActionType left, ActionType right) const
{
    return toLexActionType(left) < toLexActionType(right);
}

void QnBusinessTypesComparator::initLexOrdering()
{
    StringsHelper helper(systemContext());

    // event types to lex order
    int maxType = 0;
    QMap<QString, int> eventTypes;
    for (auto eventType: allEvents({})) //< Order is defined for deprecated event types as well.
    {
        eventTypes.insert(helper.eventName(eventType), eventType);
        if (maxType < eventType)
            maxType = eventType;
    }

    m_eventTypeToLexOrder = QVector<int>(maxType + 1, maxType); // put undefined events to the end of the list
    int count = 0;
    for (int eventType: eventTypes)
        m_eventTypeToLexOrder[eventType] = count++;

    // action types to lex order
    maxType = 0;
    QMap<QString, int> actionTypes;
    for (auto actionType: allActions())
    {
        actionTypes.insert(helper.actionName(actionType), actionType);
        if (maxType < actionType)
            maxType = actionType;
    }

    m_actionTypeToLexOrder = QVector<int>(maxType + 1, maxType); // put undefined actions to the end of the list
    count = 0;
    for (int actionType: actionTypes)
        m_actionTypeToLexOrder[actionType] = count++;
}

int QnBusinessTypesComparator::toLexEventType(EventType eventType) const
{
    return m_eventTypeToLexOrder[eventType];
}

int QnBusinessTypesComparator::toLexActionType(ActionType actionType) const
{
    return m_actionTypeToLexOrder[actionType];
}

QList<nx::vms::api::EventType> QnBusinessTypesComparator::lexSortedEvents(
    const QList<nx::vms::api::EventType>& events) const
{
    auto result = events;
    std::sort(result.begin(), result.end(),
        [this](EventType l, EventType r)
        {
            return lexicographicalLessThan(l, r);
        });
    return result;
}

QList<nx::vms::api::ActionType> QnBusinessTypesComparator::lexSortedActions(
    const QList<nx::vms::api::ActionType>& actions) const
{
    auto result = actions;
    std::sort(result.begin(), result.end(),
        [this](ActionType l, ActionType r)
        {
            return lexicographicalLessThan(l, r);
        });
    return result;
}
