#ifndef __ABSTRACT_BUSINESS_EVENT_H_
#define __ABSTRACT_BUSINESS_EVENT_H_

#include <QtCore/QByteArray>
#include <QtCore/QSharedPointer>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <business/business_fwd.h>
#include <business/business_event_parameters.h>
#include <business/business_action_parameters.h>


namespace QnBusiness
{
    bool hasChild(EventType eventType);

    QList<EventType> childEvents(EventType eventType);

    QList<EventType> allEvents();

    EventType parentEvent(EventType eventType);

    /** Check if resource required to SETUP rule on this event. */
    bool isResourceRequired(EventType eventType);


    bool hasToggleState(EventType eventType);

    QList<EventState> allowedEventStates(EventType eventType);

    bool requiresCameraResource(EventType eventType);

    bool requiresServerResource(EventType eventType);

    /** Check if camera required for this event to OCCUR. */
    bool isSourceCameraRequired(EventType eventType);

    /** Check if server required for this event to OCCUR. */
    bool isSourceServerRequired(EventType eventType);


}

/**
 * @brief The QnAbstractBusinessEvent class
 *                              Base class for business events. Contains parameters of the
 *                              occured event and methods for checking it against the rules.
 *                              No classes should directly inherit QnAbstractBusinessEvent
 *                              except the QnInstantBusinessEvent and QnProlongedBusinessEvent.
 */
class QnAbstractBusinessEvent
{
protected:
    /**
     * @brief QnAbstractBusinessEvent
     *                          Explicit constructor that MUST be overridden in descendants.
     * @param eventType         Type of the event.
     * @param resource          Resources that provided the event.
     * @param toggleState       On/off state of the event if it is toggleable.
     * @param timeStamp         Event date and time in usec from UTC.
     */
    QnAbstractBusinessEvent(QnBusiness::EventType eventType, const QnResourcePtr& resource, QnBusiness::EventState toggleState, qint64 timeStamp);

public:
    virtual ~QnAbstractBusinessEvent();

    /**
     * @brief getResource       Get resource that provided this event.
     * @return                  Shared pointer on the resource.
     */
    const QnResourcePtr& getResource() const { return m_resource; }

    /**
     * @brief getEventType      Get type of event. See QnBusiness::EventType.
     * @return                  Enumeration value on the event.
     */
    QnBusiness::EventType getEventType() const { return m_eventType; }

    /**
     * @return                  On/off state of the event.
     */
    QnBusiness::EventState getToggleState()     const { return m_toggleState; }

    /**
     * @brief isEventStateMatched Checks if event state matches to a rule. Rule will be terminated if it isn't pass any longer
     * @return                    True if event should be handled, false otherwise.
     */
    virtual bool isEventStateMatched(QnBusiness::EventState state, QnBusiness::ActionType actionType) const = 0;

    /**
     * @brief checkCondition      Checks if event params matches to a rule. Rule will not start if it isn't pass
     * @param params              Parameters of an event that are selected in rule.
     * @return                    True if event should be handled, false otherwise.
     */
    virtual bool checkEventParams(const QnBusinessEventParameters &params) const;

    /**
    * @brief getRuntimeParams     Fills event parameters structure with runtime event information
    */
    virtual QnBusinessEventParameters getRuntimeParams() const;

    /**
    * @brief getRuntimeParams     Fills event parameters structure with runtime event information
    *                             and additional information from business rule event parameters
    */
    virtual QnBusinessEventParameters getRuntimeParamsEx(
        const QnBusinessEventParameters& ruleEventParams) const;

private:
    /**
     * @brief m_eventType       Type of event. See QnBusiness::EventType.
     */
    const QnBusiness::EventType m_eventType;

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
    const QnBusiness::EventState m_toggleState;
};

Q_DECLARE_METATYPE(QnAbstractBusinessEventPtr)


#endif // __ABSTRACT_BUSINESS_EVENT_H_
