#ifndef __IP_CONFLICT_BUSINESS_EVENT_H__
#define __IP_CONFLICT_BUSINESS_EVENT_H__

#include <QHostAddress>

#include "conflict_business_event.h"
#include "core/resource/resource_fwd.h"

class QnIPConflictBusinessEvent: public QnConflictBusinessEvent
{
    typedef QnConflictBusinessEvent base_type;
public:
    QnIPConflictBusinessEvent(const QnResourcePtr& resource, const QHostAddress& address, const QStringList& macAddrList,  qint64 timeStamp);
};

typedef QSharedPointer<QnIPConflictBusinessEvent> QnIPConflictBusinessEventPtr;

#endif // __IP_CONFLICT_BUSINESS_EVENT_H__
