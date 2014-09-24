#include "ip_conflict_business_event.h"

QnIPConflictBusinessEvent::QnIPConflictBusinessEvent(
        const QnResourcePtr& resource, 
        const QHostAddress& address, 
        const QStringList& macAddrList,
        qint64 timeStamp):
        base_type(QnBusiness::CameraIpConflictEvent, resource, timeStamp, address.toString(), macAddrList)
{
}
