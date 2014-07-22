#ifndef REASONED_BUSINESS_EVENT_H
#define REASONED_BUSINESS_EVENT_H

#include "instant_business_event.h"

class QnReasonedBusinessEvent : public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    explicit QnReasonedBusinessEvent(const QnBusiness::EventType eventType,
                                     const QnResourcePtr& resource,
                                     const qint64 timeStamp,
                                     const QnBusiness::EventReason reasonCode,
                                     const QString& reasonParamsEncoded = QString());

    virtual QnBusinessEventParameters getRuntimeParams() const override;

protected:
    QnBusiness::EventReason m_reasonCode;
    QString m_reasonParamsEncoded;
};

#endif // REASONED_BUSINESS_EVENT_H
