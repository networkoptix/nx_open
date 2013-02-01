#ifndef __IP_CONFLICT_BUSINESS_EVENT_H__
#define __IP_CONFLICT_BUSINESS_EVENT_H__

#include "instant_business_event.h"
#include "core/resource/resource_fwd.h"

class QnIPConflictBusinessEvent: public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    QnIPConflictBusinessEvent(const QnResourcePtr& resource, const QHostAddress& address, const QnNetworkResourceList& cameras,  qint64 timeStamp);
    virtual QString toString() const override;
private:
    QHostAddress m_address;
    QnNetworkResourceList m_cameras;
};

typedef QSharedPointer<QnIPConflictBusinessEvent> QnIPConflictBusinessEventPtr;

#endif // __IP_CONFLICT_BUSINESS_EVENT_H__
