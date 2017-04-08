#include "business_types_comparator.h"

#include <business/business_strings_helper.h>

QnBusinessTypesComparator::QnBusinessTypesComparator(QObject* parent):
    base_type(parent)
{
    initLexOrdering();
}

bool QnBusinessTypesComparator::lexicographicalLessThan( QnBusiness::EventType left, QnBusiness::EventType right ) const {
    return toLexEventType(left) < toLexEventType(right);
}

bool QnBusinessTypesComparator::lexicographicalLessThan( QnBusiness::ActionType left, QnBusiness::ActionType right ) const {
    return toLexActionType(left) < toLexActionType(right);
}

void QnBusinessTypesComparator::initLexOrdering()
{
    QnBusinessStringsHelper helper(commonModule());

    // event types to lex order
    int maxType = 0;
    QMap<QString, int> eventTypes;
    for (auto eventType: QnBusiness::allEvents()) {
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
    for (auto actionType: QnBusiness::allActions()) {
        actionTypes.insert(helper.actionName(actionType), actionType);
        if (maxType < actionType)
            maxType = actionType;
    }

    m_actionTypeToLexOrder = QVector<int>(maxType + 1, maxType); // put undefined actions to the end of the list
    count = 0;
    for (int actionType: actionTypes)
        m_actionTypeToLexOrder[actionType] = count++;
}

int QnBusinessTypesComparator::toLexEventType( QnBusiness::EventType eventType ) const {
    return m_eventTypeToLexOrder[eventType];
}

int QnBusinessTypesComparator::toLexActionType( QnBusiness::ActionType actionType ) const {
    return m_actionTypeToLexOrder[actionType];
}

QList<QnBusiness::EventType> QnBusinessTypesComparator::lexSortedEvents() const
{
    auto events = QnBusiness::allEvents();
    std::sort(events.begin(), events.end(),
        [this](QnBusiness::EventType l, QnBusiness::EventType r)
        {
            return lexicographicalLessThan(l, r);
        });
    return events;
}

QList<QnBusiness::ActionType> QnBusinessTypesComparator::lexSortedActions() const
{
    auto actions = QnBusiness::allActions();
    std::sort(actions.begin(), actions.end(),
        [this](QnBusiness::ActionType l, QnBusiness::ActionType r)
        {
            return lexicographicalLessThan(l, r);
        });
    return actions;
}

