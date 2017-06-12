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
        case BackupFinishedEvent:
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
            result << CameraDisconnectEvent << NetworkIssueEvent << CameraIpConflictEvent << SoftwareTriggerEvent;
            break;
        case AnyServerEvent:
            result << StorageFailureEvent << ServerFailureEvent << ServerConflictEvent << ServerStartEvent << LicenseIssueEvent << BackupFinishedEvent;
            break;
        case AnyBusinessEvent:
            result << CameraMotionEvent << CameraInputEvent <<
                      AnyCameraEvent << AnyServerEvent <<
                      UserDefinedEvent << SoftwareTriggerEvent;
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
            << LicenseIssueEvent
            << BackupFinishedEvent
            << SoftwareTriggerEvent
            << UserDefinedEvent;
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
        case UserDefinedEvent:
        case SoftwareTriggerEvent:
            return true;
        default:
            return false;
        }
    }

    QList<EventState> allowedEventStates(EventType eventType)
    {
        QList<EventState> result;
        if (!hasToggleState(eventType) || eventType == UserDefinedEvent || eventType == SoftwareTriggerEvent)
            result << QnBusiness::UndefinedState;
        if (hasToggleState(eventType))
            result << QnBusiness::ActiveState << QnBusiness::InactiveState;
        return result;
    }

    bool requiresCameraResource(EventType eventType) {
        switch( eventType )
        {
        case CameraMotionEvent:
        case CameraInputEvent:
        case CameraDisconnectEvent:
        case SoftwareTriggerEvent:
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

    bool isSourceCameraRequired(EventType eventType)
    {
        switch (eventType)
        {
        case CameraMotionEvent:
        case CameraInputEvent:
        case CameraDisconnectEvent:
        case NetworkIssueEvent:
            return true;
        default:
            break;
        }
        return false;
    }

    bool isSourceServerRequired(EventType eventType)
    {
        switch (eventType)
        {
        case StorageFailureEvent:
        case BackupFinishedEvent:
        case ServerFailureEvent:
        case ServerConflictEvent:
        case ServerStartEvent:
            return true;
        default:
            break;
        }
        return false;
    }

}

QnAbstractBusinessEvent::QnAbstractBusinessEvent(QnBusiness::EventType eventType, const QnResourcePtr& resource, QnBusiness::EventState toggleState, qint64 timeStampUsec):
    m_eventType(eventType),
    m_timeStampUsec(timeStampUsec),
    m_resource(resource),
    m_toggleState(toggleState)
{
}

QnAbstractBusinessEvent::~QnAbstractBusinessEvent()
{
}

QnBusinessEventParameters QnAbstractBusinessEvent::getRuntimeParams() const {
    QnBusinessEventParameters params;
    params.eventType = m_eventType;
    params.eventTimestampUsec = m_timeStampUsec;
    params.eventResourceId = m_resource ? m_resource->getId() : QnUuid();

    return params;
}

QnBusinessEventParameters QnAbstractBusinessEvent::getRuntimeParamsEx(
    const QnBusinessEventParameters& /*ruleEventParams*/) const
{
    return getRuntimeParams();
}

bool QnAbstractBusinessEvent::checkEventParams(const QnBusinessEventParameters &params) const
{
    Q_UNUSED(params);
    return true;
}
