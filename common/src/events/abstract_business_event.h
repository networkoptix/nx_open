#ifndef __ABSTRACT_BUSINESS_EVENT_H_
#define __ABSTRACT_BUSINESS_EVENT_H_

#include <QByteArray>
#include <QSharedPointer>
#include "core/resource/resource_fwd.h"
#include "business_logic_common.h"

/*
* Base class for business events
*/

namespace BusinessEventType
{
    enum Value
    {
        BE_NotDefined,
        BE_Camera_Motion,
        BE_Camera_Input,
        BE_Camera_Disconnect,
        BE_Storage_Failure,
        BE_UserDefined = 1000
    };

    QString toString( Value val );
}


class QnAbstractBusinessEvent
{
public:
    QnAbstractBusinessEvent();
    virtual ~QnAbstractBusinessEvent() {}
    //virtual QByteArray serialize() = 0;
    //virtual bool deserialize(const QByteArray& data) = 0;

    void setDateTime(qint64 value)             { m_dateTime = value;    }
    void setResource(QnResourcePtr resource)   { m_resource = resource; }
    QnResourcePtr getResource()                { return m_resource;     }

    virtual bool checkCondition(const QnBusinessParams& params) const = 0;

    BusinessEventType::Value getEventType() const { return m_eventType; }

    void setToggleState(ToggleState value) { m_toggleState = value; }
    ToggleState getToggleState() const { return m_toggleState; }

protected:
    void setEventType(BusinessEventType::Value value) { m_eventType = value;   }

private:
    BusinessEventType::Value m_eventType;
    qint64 m_dateTime; // event date and time in usec from UTC
    QnResourcePtr m_resource; // resource that provide this event
    ToggleState m_toggleState;
};

typedef QSharedPointer<QnAbstractBusinessEvent> QnAbstractBusinessEventPtr;


#endif // __ABSTRACT_BUSINESS_EVENT_H_
