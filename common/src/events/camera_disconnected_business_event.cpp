#include "camera_disconnected_business_event.h"
#include "core/resource/resource.h"

QnCameraDisconnectedBusinessEvent::QnCameraDisconnectedBusinessEvent(
        const QnResourcePtr& mediaServerResource,
        const QnResourcePtr& cameraResource,
        qint64 timeStamp):
    base_type(BusinessEventType::BE_Camera_Disconnect,
                            mediaServerResource,
                            timeStamp),
    m_cameraResource(cameraResource)
{
}

bool QnCameraDisconnectedBusinessEvent::checkCondition(const QnBusinessParams& params) const
{
    if (!base_type::checkCondition(params))
        return false;
    QString cameraUniqId = params.value(QLatin1String("camera")).toString();
    return cameraUniqId.isEmpty() || m_cameraResource->getUniqueId() == cameraUniqId;
}

QString QnCameraDisconnectedBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    text += QString::fromLatin1("  camera %1\n").arg(m_cameraResource->getUniqueId());
    return text;
}
