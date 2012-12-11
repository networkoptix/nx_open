
#include "abstract_business_event.h"
#include "utils/common/synctime.h"

#include "core/resource/resource.h"


namespace BusinessEventType
{
    QString toString( Value val )
    {
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
            default:
                return QString::fromLatin1(val >= BE_UserDefined ? "UserDefined (%1)" : "Unknown %1").arg((int)val);
        }
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
