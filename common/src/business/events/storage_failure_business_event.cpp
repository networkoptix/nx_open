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
        int reasonCode,
        QnResourcePtr storageResource):
    base_type(BusinessEventType::BE_Storage_Failure,
                            resource,
                            timeStamp,
                            reasonCode)
{
    if (storageResource)
        m_reasonText = storageResource->getUrl();
}

QString QnStorageFailureBusinessEvent::toString() const
{
    QString reasonText;
    switch (m_reasonCode) {
        case STORAGE_IO_ERROR:
            reasonText = QObject::tr("IO error occured.");
            break;
        case STORAGE_NOT_ENOUGH_SPEED:
            reasonText = QObject::tr("Not enough HDD/SSD speed for recording.");
            break;
        default:
            break;
    }


    QString text = QnAbstractBusinessEvent::toString();
    text += QObject::tr("  reason%1\n").arg(reasonText);
    if (!m_reasonText.isEmpty())
        text += QObject::tr("  storage%1\n").arg(m_reasonText);
    else
        text += QObject::tr("  no storage for writting\n");
    return text;
}
