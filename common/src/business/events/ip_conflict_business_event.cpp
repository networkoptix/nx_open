#include "ip_conflict_business_event.h"

QnIPConflictBusinessEvent::QnIPConflictBusinessEvent(
        const QnResourcePtr& resource, 
        const QHostAddress& address, 
        const QStringList& macAddrList,
        qint64 timeStamp):
        base_type(BusinessEventType::BE_Camera_Ip_Conflict, resource, timeStamp, address.toString(), macAddrList)
{
}

QString QnIPConflictBusinessEvent::toString() const
{
    QString text = base_type::toString();
    text += QObject::tr(" conflicting cameras:");
    for (int i = 0; i < m_conflicts.size(); ++i) {
        if (i > 0)
            text += QLatin1String(", ");
        text += m_conflicts[i];
    }
    return text;
}

