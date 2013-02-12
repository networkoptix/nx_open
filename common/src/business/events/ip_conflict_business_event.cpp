#include "ip_conflict_business_event.h"
#include "core/resource/network_resource.h"

namespace QnBusinessEventRuntime {

    static QLatin1String addressStr("cameraAddress");
    static QLatin1String conflictingMac("conflictingMac");

    QHostAddress getCameraAddress(const QnBusinessParams &params) {
        // using QString as inner storage in case of future ipv6 support
        return QHostAddress(params.value(addressStr).toString());
    }

    void setCameraAddress(QnBusinessParams* params, QHostAddress value) {
        (*params)[addressStr] = value.toString();
    }

    QVariantList getConflictingCamerasMac(const QnBusinessParams &params) {
        return params.value(conflictingMac).value<QVariantList>();
    }

    void setConflictingCamerasMac(QnBusinessParams* params, QVariantList value) {
        (*params)[conflictingMac] = QVariant::fromValue(value);
    }

} //namespace QnBusinessEventRuntime

QnIPConflictBusinessEvent::QnIPConflictBusinessEvent(
        const QnResourcePtr& resource, 
        const QHostAddress& address, 
        const QStringList& cameras,
        qint64 timeStamp):
        base_type(BusinessEventType::BE_Camera_Ip_Conflict, resource, timeStamp),
        m_address(address),
        m_cameras(cameras)
{
}

QString QnIPConflictBusinessEvent::toString() const
{
    QString text = base_type::toString();
    text += QObject::tr(" conflicting ip address: %1.").arg(m_address.toString());
    text += QObject::tr(" conflicting cameras: ");
    bool firstTime = true;
    foreach(const QString& mac, m_cameras) 
    {
        if (!firstTime)
            text += QLatin1String(", ");
        text += mac;
        firstTime = false;
    }
    return text;
}

QnBusinessParams QnIPConflictBusinessEvent::getRuntimeParams() const {
    QnBusinessParams params = base_type::getRuntimeParams();
    QnBusinessEventRuntime::setCameraAddress(&params, m_address);

    QVariantList macList;
    foreach(const QString &mac, m_cameras)
        macList << mac;

    QnBusinessEventRuntime::setConflictingCamerasMac(&params, macList);
    return params;
}
