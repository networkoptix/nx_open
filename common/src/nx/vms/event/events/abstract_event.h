#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QSharedPointer>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/event/action_parameters.h>

namespace nx {
namespace vms {
namespace event {

bool hasChild(EventType eventType);

QList<EventType> childEvents(EventType eventType);

QList<EventType> allEvents();

EventType parentEvent(EventType eventType);

/** Check if resource required to SETUP rule on this event. */
bool isResourceRequired(EventType eventType);

bool hasToggleState(EventType eventType);

QList<EventState> allowedEventStates(EventType eventType);

/** Check if camera required for this event to setup a rule. */
bool requiresCameraResource(EventType eventType);

/** Check if server required for this event to setup a rule. */
bool requiresServerResource(EventType eventType);

/** Check if camera required for this event to OCCUR. Always includes requiresCameraResource(). */
bool isSourceCameraRequired(EventType eventType);

/** Check if server required for this event to OCCUR. Always includes requiresServerResource(). */
bool isSourceServerRequired(EventType eventType);

/**
 * @brief The AbstractEvent class
 *                              Base class for business events. Contains parameters of the
 *                              occured event and methods for checking it against the rules.
 *                              No classes should directly inherit AbstractEvent
 *                              except the QnInstantBusinessEvent and QnProlongedBusinessEvent.
 */
class AbstractEvent
{
protected:
    /**
     * @brief AbstractEvent
     *                          Explicit constructor that MUST be overridden in descendants.
     * @param eventType         Type of the event.
     * @param resource          Resources that provided the event.
     * @param toggleState       On/off state of the event if it is toggleable.
     * @param timeStamp         Event date and time in usec from UTC.
     */
    AbstractEvent(EventType eventType, const QnResourcePtr& resource, EventState toggleState, qint64 timeStamp);

public:
    virtual ~AbstractEvent();

    /**
     * @brief getResource       Get resource that provided this event.
     * @return                  Shared pointer on the resource.
     */
    const QnResourcePtr& getResource() const { return m_resource; }

    /**
     * @brief getEventType      Get type of event. See EventType.
     * @return                  Enumeration value on the event.
     */
    EventType getEventType() const { return m_eventType; }

    /**
     * @return                  On/off state of the event.
     */
    EventState getToggleState()     const { return m_toggleState; }

    /**
     * @brief isEventStateMatched Checks if event state matches to a rule. Rule will be terminated if it isn't pass any longer
     * @return                    True if event should be handled, false otherwise.
     */
    virtual bool isEventStateMatched(EventState state, ActionType actionType) const = 0;

    /**
     * @brief checkCondition      Checks if event params matches to a rule. Rule will not start if it isn't pass
     * @param params              Parameters of an event that are selected in rule.
     * @return                    True if event should be handled, false otherwise.
     */
    virtual bool checkEventParams(const EventParameters &params) const;

    /**
    * @brief getRuntimeParams     Fills event parameters structure with runtime event information
    */
    virtual EventParameters getRuntimeParams() const;

    /**
    * @brief getRuntimeParams     Fills event parameters structure with runtime event information
    *                             and additional information from business rule event parameters
    */
    virtual EventParameters getRuntimeParamsEx(
        const EventParameters& ruleEventParams) const;

private:
    /**
     * @brief m_eventType       Type of event. See EventType.
     */
    const EventType m_eventType;

    /**
     * @brief m_timeStamp       Event date and time in usec from UTC.
     */
    const qint64 m_timeStampUsec;

    /**
     * @brief m_resource        Resource that provide this event.
     */
    const QnResourcePtr m_resource;

    /**
     * @brief m_toggleState     State on/off for togglable events.
     */
    const EventState m_toggleState;
};

} // namespace event
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::event::AbstractEventPtr)
