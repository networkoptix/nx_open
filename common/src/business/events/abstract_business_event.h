#ifndef __ABSTRACT_BUSINESS_EVENT_H_
#define __ABSTRACT_BUSINESS_EVENT_H_

#include <QByteArray>
#include <QSharedPointer>
#include "core/resource/resource_fwd.h"
#include <business/business_logic_common.h>
#include "../business_event_parameters.h"


namespace BusinessEventType
{
    bool hasChild(Value value);

    QList<Value> childEvents(Value value);

    QString toString( Value val );

    bool isResourceRequired(Value val);

    bool hasToggleState(Value val);

    bool requiresCameraResource(Value val);

    bool requiresServerResource(Value val);
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
    QnAbstractBusinessEvent(BusinessEventType::Value eventType, const QnResourcePtr& resource, ToggleState::Value toggleState, qint64 timeStamp);

public:
    virtual ~QnAbstractBusinessEvent();

    /**
     * @brief getResource       Get resource that provided this event.
     * @return                  Shared pointer on the resource.
     */
    const QnResourcePtr& getResource() const { return m_resource; }

    /**
     * @brief getEventType      Get type of event. See BusinessEventType::Value.
     * @return                  Enumeration value on the event.
     */
    BusinessEventType::Value getEventType() const { return m_eventType; }

    /**
     * @brief getToggleState    Get on/off state of the event.
     * @return                  Enumeration value. See ToggleState::Value.
     */
    ToggleState::Value getToggleState()     const { return m_toggleState; }

    /**
     * @brief checkCondition    Checks event parameters. Default implementation includes
     *                          check agains ToggleState only.
     * @param params            Parameters of an event that are selected in rule.
     * @return                  True if event should be handled, false otherwise.
     */
    virtual bool checkCondition (ToggleState::Value state, const QnBusinessEventParameters& params) const = 0;

    virtual QnBusinessEventParameters getRuntimeParams() const;

private:
    /**
     * @brief m_eventType       Type of event. See BusinessEventType::Value.
     */
    const BusinessEventType::Value m_eventType;

    /**
     * @brief m_timeStamp       Event date and time in usec from UTC.
     */
    const qint64 m_timeStamp;

    /**
     * @brief m_resource        Resource that provide this event.
     */
    const QnResourcePtr m_resource;

    /**
     * @brief m_toggleState     State on/off for togglable events.
     */
    const ToggleState::Value m_toggleState;
};

typedef QSharedPointer<QnAbstractBusinessEvent> QnAbstractBusinessEventPtr;

Q_DECLARE_METATYPE(BusinessEventType::Value)
Q_DECLARE_METATYPE(QnAbstractBusinessEventPtr)


#endif // __ABSTRACT_BUSINESS_EVENT_H_
