#include "mserver_conflict_business_event.h"
#include "core/resource/resource.h"

QnMServerConflictBusinessEvent::QnMServerConflictBusinessEvent(
        const QnResourcePtr& mServerRes,
        qint64 timeStamp,
        const QList<QByteArray>& conflictList):
    base_type(BusinessEventType::MediaServer_Conflict,
                            mServerRes,
                            timeStamp)
{
    if (conflictList.isEmpty())
        return;
    m_source = conflictList[0]; //TODO: #GDM wtf? who formed this insane conflictList?
    for (int i = 1; i < conflictList.size(); ++i)
        m_conflicts << conflictList[i];
}
