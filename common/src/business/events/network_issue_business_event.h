#ifndef __NETWORK_ISSUE_BUSINESS_EVENT_H__
#define __NETWORK_ISSUE_BUSINESS_EVENT_H__

#include "reasoned_business_event.h"

class QnNetworkIssueBusinessEvent: public QnReasonedBusinessEvent
{
    typedef QnReasonedBusinessEvent base_type;
public:
    QnNetworkIssueBusinessEvent(const QnResourcePtr& resource,
                          qint64 timeStamp,
                          QnBusiness::EventReason reasonCode,
                          const QString &reasonText);
};

typedef QSharedPointer<QnNetworkIssueBusinessEvent> QnNetworkIssueBusinessEventPtr;

#endif // __NETWORK_ISSUE_BUSINESS_EVENT_H__
