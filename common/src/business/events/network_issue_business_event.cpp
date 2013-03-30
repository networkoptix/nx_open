#include "network_issue_business_event.h"
#include "core/resource/resource.h"

QnNetworkIssueBusinessEvent::QnNetworkIssueBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        QnBusiness::EventReason reasonCode,
        const QString &reasonText):
    base_type(BusinessEventType::Network_Issue,
              resource,
              timeStamp,
              reasonCode,
              reasonText)

{
}
