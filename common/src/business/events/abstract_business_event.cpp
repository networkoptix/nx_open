#include "abstract_business_event.h"

#include "utils/common/synctime.h"
#include "core/resource/resource.h"


namespace BusinessEventType
{

    bool hasChild(Value value)
    {
        switch (value) {
        case AnyCameraIssue:
            return true;
        case AnyServerIssue:
            return true;
        case AnyBusinessEvent:
            return true;
        default:
            break;
        }
        return false;
    }

    Value parentEvent(Value value)
    {
        switch (value) {
        case Camera_Disconnect:
        case Network_Issue:
        case Camera_Ip_Conflict:
            return AnyCameraIssue;

        case Storage_Failure:
        case MediaServer_Failure:
        case MediaServer_Conflict:
            return AnyServerIssue;

        case AnyBusinessEvent:
            return NotDefined;

        default:
            return AnyBusinessEvent;
        }
        return NotDefined;
    }

    QList<Value> childEvents(Value value)
    {
        QList<Value> result;

        switch (value) {
        case AnyCameraIssue:
            result << BusinessEventType::Camera_Disconnect << BusinessEventType::Network_Issue << BusinessEventType::Camera_Ip_Conflict;
            break;
        case AnyServerIssue:
            result << BusinessEventType::Storage_Failure << BusinessEventType::MediaServer_Failure << BusinessEventType::MediaServer_Conflict;
            break;
        case AnyBusinessEvent:
            result << BusinessEventType::Camera_Motion << BusinessEventType::Camera_Input <<
                      BusinessEventType::AnyCameraIssue << BusinessEventType::AnyServerIssue;
            break;
        default:
            break;
        }
        
        return result;
    }

    bool isResourceRequired(Value val) {
        return requiresCameraResource(val) || requiresServerResource(val);
    }

    bool hasToggleState(Value val) {
        switch( val )
        {
        case AnyBusinessEvent:
        case Camera_Motion:
        case Camera_Input:
            return true;

        case Camera_Disconnect:
        case Storage_Failure:
        case Network_Issue:
        case Camera_Ip_Conflict:
        case MediaServer_Failure:
        case MediaServer_Conflict:
            return false;

        default:
            return false;
        }
        //warning should be raised on unknown events;
        return false;
    }

    bool requiresCameraResource(Value val) {
        if (val >= Count)
            return false;
        switch( val )
        {
        case Camera_Motion:
        case Camera_Input:
        case Camera_Disconnect:
            return true;

        case Storage_Failure:
        case Network_Issue:
        case Camera_Ip_Conflict:
        case MediaServer_Failure:
        case MediaServer_Conflict:
            return false;

        default:
            return false;
        }
        return false;
    }

    bool requiresServerResource(Value val) {
        if (val >= Count)
            return false;

        switch( val )
        {
        case Storage_Failure:
            return false; //TODO: #GDM restore when will work fine

        case Camera_Motion:
        case Camera_Input:
        case Camera_Disconnect:
        case Network_Issue:
        case Camera_Ip_Conflict:
        case MediaServer_Failure:
        case MediaServer_Conflict:
            return false;

        default:
            return false;
        }
        return false;
    }
}

QnAbstractBusinessEvent::QnAbstractBusinessEvent(BusinessEventType::Value eventType, const QnResourcePtr& resource, Qn::ToggleState toggleState, qint64 timeStamp):
    m_eventType(eventType),
    m_timeStamp(timeStamp),
    m_resource(resource),
    m_toggleState(toggleState)
{
}

QnAbstractBusinessEvent::~QnAbstractBusinessEvent()
{
}

QnBusinessEventParameters QnAbstractBusinessEvent::getRuntimeParams() const {
    QnBusinessEventParameters params;
    params.setEventType(m_eventType);
    params.setEventTimestamp(m_timeStamp);
    params.setEventResourceId(m_resource ? m_resource->getId().toInt() : 0);

    return params;
}
