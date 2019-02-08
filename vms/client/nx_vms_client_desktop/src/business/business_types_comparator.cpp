#include "business_types_comparator.h"

#include <nx/vms/event/strings_helper.h>

using namespace nx;
using namespace vms::event;

QnBusinessTypesComparator::QnBusinessTypesComparator(bool onlyUserAvailableActions, QObject* parent):
    base_type(parent),
    m_onlyUserAvailableActions(onlyUserAvailableActions)
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
    StringsHelper helper(commonModule());

    // event types to lex order
    int maxType = 0;
    QMap<QString, int> eventTypes;
    for (auto eventType: allEvents())
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
    for (auto actionType: getAllActions())
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

QList<EventType> QnBusinessTypesComparator::lexSortedEvents(EventSubType subtype) const
{
    static const QList<EventType> userEvents{
        EventType::cameraMotionEvent,
        EventType::cameraInputEvent,
        EventType::softwareTriggerEvent,
        EventType::analyticsSdkEvent,
        EventType::userDefinedEvent,
        EventType::pluginEvent,
    };

    static const QList<EventType> failureEvents{
        EventType::cameraDisconnectEvent,
        EventType::storageFailureEvent,
        EventType::networkIssueEvent,
        EventType::cameraIpConflictEvent,
        EventType::serverFailureEvent,
        EventType::serverConflictEvent,
        EventType::licenseIssueEvent,
    };

    static const QList<EventType> successEvents{
        EventType::serverStartEvent,
        EventType::backupFinishedEvent,
    };

    QList<EventType> events;
    switch (subtype)
    {
        case EventSubType::user:
            events = userEvents;
            break;
        case EventSubType::failure:
            events = failureEvents;
            break;
        case EventSubType::success:
            events = successEvents;
            break;
        case EventSubType::any:
            events = allEvents();
            break;
        default:
            NX_ASSERT(false, "All values must be enumerated");
            break;
    }

    std::sort(events.begin(), events.end(),
        [this](EventType l, EventType r)
        {
            return lexicographicalLessThan(l, r);
        });
    return events;
}

QList<ActionType> QnBusinessTypesComparator::getAllActions() const
{
    return m_onlyUserAvailableActions
        ? userAvailableActions()
        : allActions();
}

QList<ActionType> QnBusinessTypesComparator::lexSortedActions(ActionSubType subtype) const
{
    static QSet<ActionType> clientsideActions{
        ActionType::openLayoutAction,
        ActionType::showOnAlarmLayoutAction,
        ActionType::fullscreenCameraAction,
        ActionType::exitFullscreenAction
    };

    auto allowedActions = getAllActions().toSet();
    switch (subtype)
    {
        case ActionSubType::server:
            allowedActions.subtract(clientsideActions);
            break;
        case ActionSubType::client:
            allowedActions.intersect(clientsideActions);
            break;
        default:
            break;
    }

    QList<ActionType> actions = allowedActions.toList();
    std::sort(actions.begin(), actions.end(),
        [this](ActionType l, ActionType r)
        {
            return lexicographicalLessThan(l, r);
        });
    return actions;
}

