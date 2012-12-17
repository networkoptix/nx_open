
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
            case BE_UserDefined:
                return QObject::tr("User Defined");
            //warning should be raised on unknown enumeration values
        }
        return QObject::tr("Unknown Event");
    }

    bool isResourceRequired(Value val) {
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
                return true;
            case BE_UserDefined:
                return false;
            //warning should be raised on unknown enumeration values
        }
        return false;
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
            case BE_UserDefined:
                return false;
        }
        //warning should be raised on unknown events;
        return false;
    }
}

namespace BusinessEventParameters {

    static QLatin1String toggleState( "toggleState" );

    ToggleState::Value getToggleState(const QnBusinessParams &params) {
        return (ToggleState::Value) params.value(toggleState, ToggleState::NotDefined).toInt();
    }

    void setToggleState(QnBusinessParams *params, ToggleState::Value value) {
        (*params)[toggleState] = (int)value;
    }

}


QnAbstractBusinessEvent::QnAbstractBusinessEvent(
        BusinessEventType::Value eventType,
        QnResourcePtr resource,
        ToggleState::Value toggleState,
        qint64 timeStamp):
    m_eventType(eventType),
    m_timeStamp(timeStamp),
    m_resource(resource),
    m_toggleState(toggleState)
{
}

QString QnAbstractBusinessEvent::toString() const
{   //Input event (input 1, on)
    QString text = QObject::tr("event type: %1\n").arg(BusinessEventType::toString(m_eventType));
    text += QObject::tr("timestamp: %1\n").arg(QDateTime::fromMSecsSinceEpoch(m_timeStamp).toString());
    return text;
}

bool QnAbstractBusinessEvent::checkCondition(const QnBusinessParams& params) const {
    ToggleState::Value toggleState = BusinessEventParameters::getToggleState(params);
    return toggleState == ToggleState::Any || toggleState == m_toggleState;
}
