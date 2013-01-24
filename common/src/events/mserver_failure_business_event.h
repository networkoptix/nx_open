#ifndef __MSERVER_FAILURE_BUSINESS_EVENT_H__
#define __MSERVER_FAILURE_BUSINESS_EVENT_H__

#include "instant_business_event.h"

class QnMServerFailureBusinessEvent: public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    QnMServerFailureBusinessEvent(const QnResourcePtr& resource, qint64 timeStamp);

    virtual QString toString() const override;
};

typedef QSharedPointer<QnMServerFailureBusinessEvent> QnMServerFailureBusinessEventPtr;

#endif // __MSERVER_FAILURE_BUSINESS_EVENT_H__
