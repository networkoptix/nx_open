#include "business_types_comparator.h"

#include <nx/vms/event/strings_helper.h>

using namespace nx;

QnBusinessTypesComparator::QnBusinessTypesComparator(QObject* parent):
    base_type(parent)
{
    initLexOrdering();
}

bool QnBusinessTypesComparator::lexicographicalLessThan( vms::event::EventType left, vms::event::EventType right ) const {
    return toLexEventType(left) < toLexEventType(right);
}

bool QnBusinessTypesComparator::lexicographicalLessThan( vms::event::ActionType left, vms::event::ActionType right ) const {
    return toLexActionType(left) < toLexActionType(right);
}

void QnBusinessTypesComparator::initLexOrdering()
{
    vms::event::StringsHelper helper(commonModule());

    // event types to lex order
    int maxType = 0;
    QMap<QString, int> eventTypes;
    for (auto eventType: vms::event::allEvents()) {
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
    for (auto actionType: vms::event::allActions()) {
        actionTypes.insert(helper.actionName(actionType), actionType);
        if (maxType < actionType)
            maxType = actionType;
    }

    m_actionTypeToLexOrder = QVector<int>(maxType + 1, maxType); // put undefined actions to the end of the list
    count = 0;
    for (int actionType: actionTypes)
        m_actionTypeToLexOrder[actionType] = count++;
}

int QnBusinessTypesComparator::toLexEventType( vms::event::EventType eventType ) const {
    return m_eventTypeToLexOrder[eventType];
}

int QnBusinessTypesComparator::toLexActionType( vms::event::ActionType actionType ) const {
    return m_actionTypeToLexOrder[actionType];
}

QList<vms::event::EventType> QnBusinessTypesComparator::lexSortedEvents() const
{
    auto events = vms::event::allEvents();
    std::sort(events.begin(), events.end(),
        [this](vms::event::EventType l, vms::event::EventType r)
        {
            return lexicographicalLessThan(l, r);
        });
    return events;
}

QList<vms::event::ActionType> QnBusinessTypesComparator::lexSortedActions() const
{
    auto actions = vms::event::allActions();
    std::sort(actions.begin(), actions.end(),
        [this](vms::event::ActionType l, vms::event::ActionType r)
        {
            return lexicographicalLessThan(l, r);
        });
    return actions;
}

