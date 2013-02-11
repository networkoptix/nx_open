#ifndef __NETWORK_ISSUE_BUSINESS_EVENT_H__
#define __NETWORK_ISSUE_BUSINESS_EVENT_H__

#include "instant_business_event.h"

class QnNetworkIssueBusinessEvent: public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    QnNetworkIssueBusinessEvent(const QnResourcePtr& resource,
                          qint64 timeStamp,
                          const QString& reason);

    virtual QString toString() const override;
    virtual QnBusinessParams getRuntimeParams() const override;
private:
    QString m_reason;
};

typedef QSharedPointer<QnNetworkIssueBusinessEvent> QnNetworkIssueBusinessEventPtr;

#endif // __NETWORK_ISSUE_BUSINESS_EVENT_H__
