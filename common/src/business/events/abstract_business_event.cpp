#include "abstract_business_event.h"

#include "utils/common/synctime.h"
#include "core/resource/resource.h"


namespace BusinessEventType
{
    QString toString( Value val )  {
        if (val >= UserDefined)
            return QObject::tr("User Defined (%1)").arg((int)val - (int)UserDefined);

        if (val >= Count)
            return QString();

        switch( val )
        {
        case Camera_Motion:
            return QObject::tr("Motion on Camera");
        case Camera_Input:
            return QObject::tr("Input Signal on Camera");
        case Camera_Disconnect:
            return QObject::tr("Camera Disconnected");
        case Storage_Failure:
            return QObject::tr("Storage Failure");
        case Network_Issue:
            return QObject::tr("Network Issue");
        case Camera_Ip_Conflict:
            return QObject::tr("Camera IP Conflict");
        case MediaServer_Failure:
            return QObject::tr("Media Server Failure");
        case MediaServer_Conflict:
            return QObject::tr("Media Server Conflict");
        default:
            return QString();
        }
        //return QObject::tr("Unknown Event");
    }

    bool isResourceRequired(Value val) {
        return requiresCameraResource(val) || requiresServerResource(val);
    }

    bool hasToggleState(Value val) {
        if (val >= Count)
            return false;

        switch( val )
        {
        case Camera_Motion:
            return true;
        case Camera_Input:
            return true;
        case Camera_Disconnect:
            return false;
        case Storage_Failure:
            return false;
        case Network_Issue:
            return false;
        case Camera_Ip_Conflict:
            return false;
        case MediaServer_Failure:
            return false;
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
            return true;
        case Camera_Input:
            return true;
        case Camera_Disconnect:
            return true;
        case Storage_Failure:
            return false;
        case Network_Issue:
            return false;
        case Camera_Ip_Conflict:
            return false;
        case MediaServer_Failure:
            return false;
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
        case Camera_Motion:
            return false;
        case Camera_Input:
            return false;
        case Camera_Disconnect:
            return false;
        case Storage_Failure:
            return false; //TODO: #GDM restore when will work fine
        case Network_Issue:
            return false;
        case Camera_Ip_Conflict:
            return false;
        case MediaServer_Failure:
            return false;
        case MediaServer_Conflict:
            return false;
        default:
            return false;
        }
        return false;
    }
}

namespace QnBusinessEventRuntime {

    static QLatin1String typeStr("eventType");
    static QLatin1String timestampStr("eventTimestamp");
    static QLatin1String resourceIdStr("eventResourceId");

    BusinessEventType::Value getEventType(const QnBusinessParams &params) {
        return (BusinessEventType::Value) params.value(typeStr, BusinessEventType::NotDefined).toInt();
    }

    void setEventType(QnBusinessParams* params, BusinessEventType::Value value) {
        (*params)[typeStr] = (int)value;
    }

    qint64 getEventTimestamp(const QnBusinessParams &params) {
        return params.value(timestampStr, BusinessEventType::NotDefined).toLongLong();
    }

    void setEventTimestamp(QnBusinessParams* params, qint64 value) {
        (*params)[timestampStr] = value;
    }

    int getEventResourceId(const QnBusinessParams &params) {
        return params.value(resourceIdStr).toInt();
    }

    void setEventResourceId(QnBusinessParams* params, int value) {
        (*params)[resourceIdStr] = value;
    }

}

QnAbstractBusinessEvent::QnAbstractBusinessEvent(BusinessEventType::Value eventType, const QnResourcePtr& resource, ToggleState::Value toggleState, qint64 timeStamp):
    m_eventType(eventType),
    m_timeStamp(timeStamp),
    m_resource(resource),
    m_toggleState(toggleState)
{
}

QnAbstractBusinessEvent::~QnAbstractBusinessEvent()
{
}

QnBusinessParams QnAbstractBusinessEvent::getRuntimeParams() const {
    QnBusinessParams params;
    QnBusinessEventRuntime::setEventType(&params, m_eventType);
    QnBusinessEventRuntime::setEventTimestamp(&params, m_timeStamp);
    QnBusinessEventRuntime::setEventResourceId(&params, m_resource ? m_resource->getId().toInt() : 0);
    QnBusinessEventRuntime::setParamsKey(&params, QString::number(m_eventType)); //default value, will be overwritten in required cases

    return params;
}
