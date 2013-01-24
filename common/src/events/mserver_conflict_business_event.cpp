#include "mserver_conflict_business_event.h"
#include "core/resource/resource.h"

QnMServerConflictBusinessEvent::QnMServerConflictBusinessEvent(
        const QnResourcePtr& mServerRes,
        qint64 timeStamp,
        const QList<QByteArray>& conflictList):
    base_type(BusinessEventType::BE_MediaServer_Conflict,
                            mServerRes,
                            timeStamp),
    m_conflictList(conflictList)
{
}

QString QnMServerConflictBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    if (!m_conflictList.isEmpty()) {
        text += QObject::tr("  several media servers are running on different EC! current EC connection %1 conflict with EC(s): ").arg(QString::fromUtf8(m_conflictList[0]));
        for (int i = 1; i < m_conflictList.size(); ++i) {
            if (i > 1)
                text += QLatin1String(", ");
            text += QString::fromLocal8Bit(m_conflictList[i]);
        }
    }
    return text;
}
