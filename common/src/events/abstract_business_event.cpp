
#include "abstract_business_event.h"
#include "utils/common/synctime.h"


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


QnAbstractBusinessEvent::QnAbstractBusinessEvent():
    m_toggleState(ToggleState::NotDefined),
    m_eventType(BusinessEventType::BE_NotDefined),
    m_dateTime(qnSyncTime->currentUSecsSinceEpoch())
{
}

QString QnAbstractBusinessEvent::toString() const
{   //Input event (input 1, on)
    QString text = BusinessEventType::toString(m_eventType) + QLatin1String(" event\n");
    text += QString::fromLatin1("  timestamp: %2\n").arg(QDateTime::fromMSecsSinceEpoch(m_dateTime).toString());
    return text;
}
