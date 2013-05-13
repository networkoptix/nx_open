#ifndef REASONED_BUSINESS_EVENT_H
#define REASONED_BUSINESS_EVENT_H

#include "instant_business_event.h"

namespace QnBusinessEventRuntime {

    QnBusiness::EventReason getReasonCode(const QnBusinessParams &params);
    void setReasonCode(QnBusinessParams* params, QnBusiness::EventReason value);

    QString getReasonText(const QnBusinessParams &params);
    void setReasonText(QnBusinessParams* params, QString value);
}

class QnReasonedBusinessEvent : public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    explicit QnReasonedBusinessEvent(const BusinessEventType::Value eventType,
                                     const QnResourcePtr& resource,
                                     const qint64 timeStamp,
                                     const QnBusiness::EventReason reasonCode,
                                     const QString& reasonText = QString());

    virtual QnBusinessParams getRuntimeParams() const override;
protected:
    QnBusiness::EventReason m_reasonCode;
    QString m_reasonText;
};

#endif // REASONED_BUSINESS_EVENT_H
