#include "mserver_conflict_business_event.h"
#include "core/resource/resource.h"

QnMServerConflictBusinessEvent::QnMServerConflictBusinessEvent(
        const QnResourcePtr& mServerRes,
        qint64 timeStamp,
        const QList<QByteArray>& conflictList):
    base_type(BusinessEventType::BE_MediaServer_Conflict,
                            mServerRes,
                            timeStamp)
{
    if (conflictList.isEmpty())
        return;
    m_source = QString::fromUtf8(conflictList[0]); //TODO: #GDM wtf? who formed this insane conflictList?
    for (int i = 1; i < conflictList.size(); ++i)
        m_conflicts << QString::fromLocal8Bit(conflictList[i]);
}

QString QnMServerConflictBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    if (!m_source.isEmpty()) {
        text += QObject::tr("  several media servers are running on different EC! current EC connection %1 conflict with EC(s): ").arg(m_source);
        for (int i = 0; i < m_conflicts.size(); ++i) {
            if (i > 0)
                text += QLatin1String(", ");
            text += m_conflicts[i];
        }
    }
    return text;
}
