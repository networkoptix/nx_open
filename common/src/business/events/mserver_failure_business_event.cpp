#include "mserver_failure_business_event.h"
#include "core/resource/resource.h"

QnMServerFailureBusinessEvent::QnMServerFailureBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        const QString& reason):
    base_type(BusinessEventType::BE_MediaServer_Failure,
                            resource,
                            timeStamp),
                            m_reason(reason)
{
}

QString QnMServerFailureBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    text += QObject::tr(". Reason: %1").arg(m_reason);
    return text;
}

QnBusinessParams QnMServerFailureBusinessEvent::getRuntimeParams() const {
    QnBusinessParams params = base_type::getRuntimeParams();
    QnBusinessEventRuntime::setEventReason(&params, m_reason);
    return params;
}
