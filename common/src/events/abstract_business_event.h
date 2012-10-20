#ifndef __ABSTRACT_BUSINESS_EVENT_H_
#define __ABSTRACT_BUSINESS_EVENT_H_

#include <QByteArray>
#include <QSharedPointer>
#include "core/resource/resource_fwd.h"

/*
* Base class for business events
*/

enum BusinessEventType {
        BE_NotDefined,
        BE_Camera_Motion,
        BE_Camera_Input,
        BE_Camera_Disconnect,
        BE_Storage_Failure,
        BE_UserDefined = 1000
};

class QnAbstractBusinessEvent
{
public:
    QnAbstractBusinessEvent();
    virtual ~QnAbstractBusinessEvent() {}
    virtual QByteArray serialize() = 0;
    virtual bool deserialize(const QByteArray& data) = 0;

    void setDateTime(qint64 value)             { m_dateTime = value;    }
    void setResource(QnResourcePtr resource)   { m_resource = resource; }
    QnResourcePtr getResource()                { return m_resource;     }
protected:
    void setEventType(BusinessEventType value) { m_eventType = value;   }
private:
    int m_eventType;
    qint64 m_dateTime; // event date and time in usec from UTC
    QnResourcePtr m_resource; // resource that provide this event
};

typedef QSharedPointer<QnAbstractBusinessEvent> QnAbstractBusinessEventPtr;


#endif // __ABSTRACT_BUSINESS_EVENT_H_
