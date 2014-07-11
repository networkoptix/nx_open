#include "mserver_conflict_business_event.h"
#include "core/resource/resource.h"

QnMServerConflictBusinessEvent::QnMServerConflictBusinessEvent(
        const QnResourcePtr& mServerRes,
        qint64 timeStamp,
        const QList<QByteArray>& conflictList):
    base_type(QnBusiness::ServerConflictEvent,
                            mServerRes,
                            timeStamp)
{
    if (conflictList.isEmpty())
        return;
    m_source = QString::fromUtf8(conflictList[0]); //TODO: #GDM #Business wtf? who formed this insane conflictList?
    for (int i = 1; i < conflictList.size(); ++i)
        m_conflicts << QString::fromUtf8(conflictList[i]);
}
