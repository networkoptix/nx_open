#include "ip_conflict_business_event.h"
#include "core/resource/network_resource.h"

namespace QnBusinessEventRuntime {

    static QLatin1String addressStr("cameraAddress");
    static QLatin1String macAddrs("macAddrs");

    QHostAddress getCameraAddress(const QnBusinessParams &params) {
        // using QString as inner storage in case of future ipv6 support
        return QHostAddress(params.value(addressStr).toString());
    }

    void setCameraAddress(QnBusinessParams* params, QHostAddress value) {
        (*params)[addressStr] = value.toString();
    }

    QStringList getConflictingMacAddrs(const QnBusinessParams &params) {
        return params.value(macAddrs).value<QStringList>();
    }

    void setConflictingMacAddrs(QnBusinessParams* params, QStringList value) {
        (*params)[macAddrs] = QVariant::fromValue(value);
    }

} //namespace QnBusinessEventRuntime

QnIPConflictBusinessEvent::QnIPConflictBusinessEvent(
        const QnResourcePtr& resource, 
        const QHostAddress& address, 
        const QStringList& macAddrList,
        qint64 timeStamp):
        base_type(BusinessEventType::BE_Camera_Ip_Conflict, resource, timeStamp),
        m_address(address),
        m_macAddrList(macAddrList)
{
}

QString QnIPConflictBusinessEvent::toString() const
{
    QString text = base_type::toString();
    text += QObject::tr(" conflicting cameras:");
    for (int i = 0; i < m_macAddrList.size(); ++i) {
        if (i > 0)
            text += QLatin1String(", ");
        text += m_macAddrList[i];
    }
    return text;
}

QnBusinessParams QnIPConflictBusinessEvent::getRuntimeParams() const {
    QnBusinessParams params = base_type::getRuntimeParams();
    QnBusinessEventRuntime::setCameraAddress(&params, m_address);
    QnBusinessEventRuntime::setConflictingMacAddrs(&params, m_macAddrList);
    return params;
}
