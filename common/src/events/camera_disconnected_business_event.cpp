#include "camera_disconnected_business_event.h"
#include "core/resource/resource.h"

QnCameraDisconnectedBusinessEvent::QnCameraDisconnectedBusinessEvent(
        QnResourcePtr mediaServerResource,
        QnResourcePtr cameraResource,
        qint64 timeStamp):
    QnAbstractBusinessEvent(BusinessEventType::BE_Camera_Disconnect,
                            mediaServerResource,
                            ToggleState::NotDefined,
                            timeStamp)
{
}

bool QnCameraDisconnectedBusinessEvent::checkCondition(const QnBusinessParams& params) const
{
    QString cameraUniqId = params.value(QLatin1String("camera")).toString();
    return cameraUniqId.isEmpty() || m_cameraResource->getUniqueId() == cameraUniqId;
}

QString QnCameraDisconnectedBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    text += QString::fromLatin1("  camera %1\n").arg(m_cameraResource->getUniqueId());
    return text;
}
