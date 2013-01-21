#include "ip_conflict_business_event.h"
#include "core/resource/network_resource.h"

QnIPConflictBusinessEvent::QnIPConflictBusinessEvent(
        const QnResourcePtr& resource, 
        const QHostAddress& address, 
        const QnNetworkResourceList& cameras,
        qint64 timeStamp):
        base_type(BusinessEventType::BE_Camera_Ip_Conflict, resource, timeStamp),
        m_address(address),
        m_cameras(cameras)
{
}

QString QnIPConflictBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    text += QObject::tr(" conflicting cameras:");
    for (int i = 0; i < m_cameras.size(); ++i) {
        if (i > 0)
            text += QLatin1String(", ");
        text += m_cameras[i]->getMAC().toString();
    }
    return text;
}
