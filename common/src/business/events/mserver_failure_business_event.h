#ifndef __MSERVER_FAILURE_BUSINESS_EVENT_H__
#define __MSERVER_FAILURE_BUSINESS_EVENT_H__

#include "reasoned_business_event.h"

class QnMServerFailureBusinessEvent: public QnReasonedBusinessEvent
{
    typedef QnReasonedBusinessEvent base_type;
public:
    QnMServerFailureBusinessEvent(const QnResourcePtr& resource, qint64 timeStamp, int reasonCode);

    virtual QString toString() const override;
};

typedef QSharedPointer<QnMServerFailureBusinessEvent> QnMServerFailureBusinessEventPtr;

#endif // __MSERVER_FAILURE_BUSINESS_EVENT_H__
