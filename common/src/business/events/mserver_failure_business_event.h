#ifndef __MSERVER_FAILURE_BUSINESS_EVENT_H__
#define __MSERVER_FAILURE_BUSINESS_EVENT_H__

#include "reasoned_business_event.h"

class QnMServerFailureBusinessEvent: public QnReasonedBusinessEvent
{
    typedef QnReasonedBusinessEvent base_type;
public:
    QnMServerFailureBusinessEvent(const QnResourcePtr& resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);
};

typedef QSharedPointer<QnMServerFailureBusinessEvent> QnMServerFailureBusinessEventPtr;

class QnMServerStartedBusinessEvent: public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    QnMServerStartedBusinessEvent(const QnResourcePtr& resource, qint64 timeStamp);
};

typedef QSharedPointer<QnMServerStartedBusinessEvent> QnMServerStartedBusinessEventPtr;

class QnLicenseIssueBusinessEvent: public QnReasonedBusinessEvent
{
    typedef QnReasonedBusinessEvent base_type;
public:
    QnLicenseIssueBusinessEvent(const QnResourcePtr& resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);
};

typedef QSharedPointer<QnLicenseIssueBusinessEvent> QnLicenseIssueBusinessEventPtr;

#endif // __MSERVER_FAILURE_BUSINESS_EVENT_H__
