#include "mserver_failure_business_event.h"
#include "core/resource/resource.h"

QnMServerFailureBusinessEvent::QnMServerFailureBusinessEvent(const QnResourcePtr& resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText):
    base_type(QnBusiness::ServerFailureEvent,resource, timeStamp, reasonCode, reasonText)
{
}

QnMServerStartedBusinessEvent::QnMServerStartedBusinessEvent(const QnResourcePtr& resource, qint64 timeStamp):
    base_type(QnBusiness::ServerStartEvent, resource, timeStamp)
{
}

QnLicenseIssueBusinessEvent::QnLicenseIssueBusinessEvent(const QnResourcePtr& resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText):
    base_type(QnBusiness::LicenseIssueEvent, resource, timeStamp, reasonCode, reasonText)
{
}
