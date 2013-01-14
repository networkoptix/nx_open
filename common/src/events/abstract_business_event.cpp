#include "abstract_business_event.h"

#include "utils/common/synctime.h"
#include "core/resource/resource.h"


namespace BusinessEventType
{
    QString toString( Value val )  {
        if (val > BE_UserDefined)
            return QObject::tr("User Defined (%1)").arg((int)val);

        switch( val )
        {
            case BE_NotDefined:
                return QLatin1String("---");
            case BE_Camera_Motion:
                return QObject::tr("Motion on Camera");
            case BE_Camera_Input:
                return QObject::tr("Input Signal on Camera");
            case BE_Camera_Disconnect:
                return QObject::tr("Camera Disconnected");
            case BE_Storage_Failure:
                return QObject::tr("Storage Failure");
            case BE_Network_Issue:
                return QObject::tr("Network Issue");
            case BE_Camera_Ip_Conflict:
                return QObject::tr("Camera IP Conflict");
            case BE_UserDefined:
                return QObject::tr("User Defined");
            //warning should be raised on unknown enumeration values
        }
        return QObject::tr("Unknown Event");
    }

    bool isResourceRequired(Value val) {
        return requiresCameraResource(val) || requiresServerResource(val);
    }

    bool hasToggleState(Value val) {
        if (val > BE_UserDefined)
            return false;

        switch( val )
        {
            case BE_NotDefined:
                return false;
            case BE_Camera_Motion:
                return true;
            case BE_Camera_Input:
                return true;
            case BE_Camera_Disconnect:
                return false;
            case BE_Storage_Failure:
                return false;
            case BE_Network_Issue:
                return false;
            case BE_Camera_Ip_Conflict:
                return false;
            case BE_UserDefined:
                return false;
        }
        //warning should be raised on unknown events;
        return false;
    }

    bool requiresCameraResource(Value val) {
        if (val > BE_UserDefined)
            return false;

        switch( val )
        {
            case BE_NotDefined:
                return false;
            case BE_Camera_Motion:
                return true;
            case BE_Camera_Input:
                return true;
            case BE_Camera_Disconnect:
                return true;
            case BE_Storage_Failure:
                return false;
            case BE_Network_Issue:
                return false;
            case BE_Camera_Ip_Conflict:
                return false;
            case BE_UserDefined:
                return false;
            //warning should be raised on unknown enumeration values
        }
        return false;
    }

    bool requiresServerResource(Value val) {
        if (val > BE_UserDefined)
            return false;

        switch( val )
        {
            case BE_NotDefined:
                return false;
            case BE_Camera_Motion:
                return false;
            case BE_Camera_Input:
                return false;
            case BE_Camera_Disconnect:
                return false;
            case BE_Storage_Failure:
                return false; //TODO: #GDM restore when will work fine
            case BE_Network_Issue:
                return false;
            case BE_Camera_Ip_Conflict:
                return false;
            case BE_UserDefined:
                return false;
            //warning should be raised on unknown enumeration values
        }
        return false;
    }
}

namespace QnBusinessEventRuntime {

    static QLatin1String typeStr("eventType");
    static QLatin1String nameStr("eventResourceName");
    static QLatin1String urlStr("eventResourceUrl");
    static QLatin1String descriptionStr("eventDescription");

    BusinessEventType::Value getEventType(const QnBusinessParams &params) {
        return (BusinessEventType::Value) params.value(typeStr, BusinessEventType::BE_NotDefined).toInt();
    }

    void setEventType(QnBusinessParams* params, BusinessEventType::Value value) {
        (*params)[typeStr] = (int)value;
    }

    QString getEventResourceName(const QnBusinessParams &params) {
        return params.value(nameStr, QString()).toString();
    }

    void setEventResourceName(QnBusinessParams* params, QString value) {
        (*params)[nameStr] = value;
    }

    QString getEventResourceUrl(const QnBusinessParams &params) {
        return params.value(urlStr, QString()).toString();
    }

    void setEventResourceUrl(QnBusinessParams* params, QString value) {
        (*params)[urlStr] = value;
    }

    QString getEventDescription(const QnBusinessParams &params) {
        return params.value(descriptionStr, QString()).toString();
    }

    void setEventDescription(QnBusinessParams* params, QString value) {
        (*params)[descriptionStr] = value;
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

QString QnAbstractBusinessEvent::toString() const
{   //Input event (input 1, on)
    QString text = QObject::tr("event type: %1\n").arg(BusinessEventType::toString(m_eventType));
    text += QObject::tr("timestamp: %1\n").arg(QDateTime::fromMSecsSinceEpoch(m_timeStamp/1000).toString());
    return text;
}

QnBusinessParams QnAbstractBusinessEvent::getRuntimeParams() const {
    QnBusinessParams params;
    QnBusinessEventRuntime::setEventType(&params, m_eventType);
    QnBusinessEventRuntime::setEventDescription(&params, toString());
    QnBusinessEventRuntime::setEventResourceName(&params, m_resource ? m_resource->getName() : QString());
    QnBusinessEventRuntime::setEventResourceUrl(&params, m_resource ? m_resource->getUrl() : QString());

    return params;
}
