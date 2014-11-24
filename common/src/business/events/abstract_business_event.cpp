#include "abstract_business_event.h"

#include "utils/common/synctime.h"
#include "core/resource/resource.h"


namespace QnBusiness
{

    bool hasChild(EventType eventType)
    {
        switch (eventType) {
        case AnyCameraEvent:
            return true;
        case AnyServerEvent:
            return true;
        case AnyBusinessEvent:
            return true;
        default:
            break;
        }
        return false;
    }

    EventType parentEvent(EventType eventType)
    {
        switch (eventType) {
        case CameraDisconnectEvent:
        case NetworkIssueEvent:
        case CameraIpConflictEvent:
            return AnyCameraEvent;

        case StorageFailureEvent:
        case ServerFailureEvent:
        case ServerConflictEvent:
        case ServerStartEvent:
        case LicenseIssueEvent:
            return AnyServerEvent;

        case AnyBusinessEvent:
            return UndefinedEvent;

        default:
            return AnyBusinessEvent;
        }
        return UndefinedEvent;
    }

    QList<EventType> childEvents(EventType eventType)
    {
        QList<EventType> result;

        switch (eventType) {
        case AnyCameraEvent:
            result << CameraDisconnectEvent << NetworkIssueEvent << CameraIpConflictEvent;
            break;
        case AnyServerEvent:
            result << StorageFailureEvent << ServerFailureEvent << ServerConflictEvent << ServerStartEvent << LicenseIssueEvent;
            break;
        case AnyBusinessEvent:
            result << CameraMotionEvent << CameraInputEvent <<
                      AnyCameraEvent << AnyServerEvent;
            break;
        default:
            break;
        }
        
        return result;
    }

    QList<EventType> allEvents() {
        QList<EventType> result;
        result 
            << CameraMotionEvent
            << CameraInputEvent
            << CameraDisconnectEvent 
            << StorageFailureEvent 
            << NetworkIssueEvent 
            << CameraIpConflictEvent 
            << ServerFailureEvent 
            << ServerConflictEvent 
            << ServerStartEvent 
            << LicenseIssueEvent;
        return result;
    }

    bool isResourceRequired(EventType eventType) {
        return requiresCameraResource(eventType) || requiresServerResource(eventType);
    }

    bool hasToggleState(EventType eventType) {
        switch( eventType )
        {
        case AnyBusinessEvent:
        case CameraMotionEvent:
        case CameraInputEvent:
            return true;
        default:
            return false;
        }
    }

    bool requiresCameraResource(EventType eventType) {
        switch( eventType )
        {
        case CameraMotionEvent:
        case CameraInputEvent:
        case CameraDisconnectEvent:
            return true;
        default:
            return false;
        }
    }

    bool requiresServerResource(EventType eventType) {
        switch( eventType )
        {
        case StorageFailureEvent:
            return false; //TODO: #GDM #Business restore when will work fine
        default:
            return false;
        }
        return false;
    }
}

QnAbstractBusinessEvent::QnAbstractBusinessEvent(QnBusiness::EventType eventType, const QnResourcePtr& resource, QnBusiness::EventState toggleState, qint64 timeStamp):
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
    params.setEventResourceId(m_resource ? m_resource->getId() : QnUuid());

    return params;
}
