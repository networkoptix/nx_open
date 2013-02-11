#include "storage_failure_business_event.h"
#include "core/resource/resource.h"

namespace QnBusinessEventRuntime
{
    static QLatin1String storageUrlStr("storageUrl");

    QString getStorageResourceUrl(const QnBusinessParams &params)
    {
        return params.value(storageUrlStr, QString()).toString();
    }

    void setStorageResourceUrl(QnBusinessParams* params, QString value)
    {
        (*params)[storageUrlStr] = value;
    }
}

QnStorageFailureBusinessEvent::QnStorageFailureBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        QnResourcePtr storageResource,
        const QString& reason):
    base_type(BusinessEventType::BE_Storage_Failure,
                            resource,
                            timeStamp),
    m_reason(reason),
    m_storageResource(storageResource)
{
}

QString QnStorageFailureBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    text += QObject::tr("  reason%1\n").arg(m_reason);
    if (m_storageResource)
        text += QObject::tr("  storage%1\n").arg(m_storageResource->getUrl());
    else
        text += QObject::tr("  no storage for writting\n");
    return text;
}

QnBusinessParams QnStorageFailureBusinessEvent::getRuntimeParams() const {
    QnBusinessParams params = base_type::getRuntimeParams();
    QnBusinessEventRuntime::setEventReason(&params, m_reason);
    if (m_storageResource)
        QnBusinessEventRuntime::setStorageResourceUrl(&params, m_storageResource->getUrl());
    return params;
}
