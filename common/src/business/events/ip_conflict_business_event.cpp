#include "ip_conflict_business_event.h"
#include "core/resource/network_resource.h"

namespace QnBusinessEventRuntime {

    static QLatin1String addressStr("cameraAddress");
    static QLatin1String conflictingIds("conflictingIds");

    QHostAddress getCameraAddress(const QnBusinessParams &params) {
        // using QString as inner storage in case of future ipv6 support
        return QHostAddress(params.value(addressStr).toString());
    }

    void setCameraAddress(QnBusinessParams* params, QHostAddress value) {
        (*params)[addressStr] = value.toString();
    }

    QVariantList getConflictingCamerasIds(const QnBusinessParams &params) {
        return params.value(conflictingIds).value<QVariantList>();
    }

    void setConflictingCamerasIds(QnBusinessParams* params, QVariantList value) {
        (*params)[conflictingIds] = QVariant::fromValue(value);
    }

} //namespace QnBusinessEventRuntime

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
    QString text = base_type::toString();
    text += QObject::tr(" conflicting cameras:");
    for (int i = 0; i < m_cameras.size(); ++i) {
        if (i > 0)
            text += QLatin1String(", ");
        text += m_cameras[i]->getMAC().toString();
    }
    return text;
}

QnBusinessParams QnIPConflictBusinessEvent::getRuntimeParams() const {
    QnBusinessParams params = base_type::getRuntimeParams();
    QnBusinessEventRuntime::setCameraAddress(&params, m_address);

    QVariantList ids;
    foreach(const QnResourcePtr &camera, m_cameras)
        ids << camera->getId().toInt();

    QnBusinessEventRuntime::setConflictingCamerasIds(&params, ids);
    return params;
}
