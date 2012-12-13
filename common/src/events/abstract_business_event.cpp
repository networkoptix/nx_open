
#include "abstract_business_event.h"
#include "utils/common/synctime.h"

#include "core/resource/resource.h"


namespace BusinessEventType
{
    QString toString( Value val )  {
        if (val > BE_UserDefined)
            return QString(QLatin1String("UserDefined (%1)")).arg((int)val);

        switch( val )
        {
            case BE_NotDefined:
                return QLatin1String("NotDefined");
            case BE_Camera_Motion:
                return QLatin1String("Camera_Motion");
            case BE_Camera_Input:
                return QLatin1String("Camera_Input");
            case BE_Camera_Disconnect:
                return QLatin1String("Camera_Disconnect");
            case BE_Storage_Failure:
                return QLatin1String("Storage_Failure");
            case BE_UserDefined:
                return QLatin1String("UserDefined");
        }
        //warning should be raised on unknown events;
        return QLatin1String("UnknownEvent");
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
        }
        //warning should be raised on unknown events;
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
    QString text = QString::fromLatin1("  event type: %1\n").arg(BusinessEventType::toString(m_eventType));
    text += QString::fromLatin1("  timestamp: %2\n").arg(QDateTime::fromMSecsSinceEpoch(m_timeStamp).toString());
    return text;
}

bool QnAbstractBusinessEvent::checkCondition(const QnBusinessParams& params) const {
    QVariant toggleState = getParameter(params, BusinessEventParameters::toggleState);
    if (!toggleState.isValid())
        return true;
    ToggleState::Value requiredToggleState = ToggleState::fromString(toggleState.toString().toLatin1().data());
    return requiredToggleState == ToggleState::Any || requiredToggleState == m_toggleState;
}

QVariant QnAbstractBusinessEvent::getParameter(const QnBusinessParams &params, const QString &paramName) {
    QnBusinessParams::const_iterator paramIter = params.find(paramName);
    if( paramIter == params.end() )
        return QVariant();
    return paramIter.value();
}
