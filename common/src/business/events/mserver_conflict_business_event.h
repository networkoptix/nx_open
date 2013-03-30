#ifndef __MSERVER_CONFLICT_BUSINESS_EVENT_H__
#define __MSERVER_CONFLICT_BUSINESS_EVENT_H__

#include "conflict_business_event.h"
#include "core/resource/resource_fwd.h"

class QnMServerConflictBusinessEvent: public QnConflictBusinessEvent
{
    typedef QnConflictBusinessEvent base_type;
public:
    QnMServerConflictBusinessEvent(const QnResourcePtr& mServerRes, qint64 timeStamp, const QList<QByteArray>& conflictList);
};

typedef QSharedPointer<QnMServerConflictBusinessEvent> QnMServerConflictBusinessEventPtr;

#endif // __MSERVER_CONFLICT_BUSINESS_EVENT_H__
