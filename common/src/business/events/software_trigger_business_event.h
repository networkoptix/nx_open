#pragma once

#include <business/events/prolonged_business_event.h>

class QnUuid;

class QnSoftwareTriggerEvent: public QnProlongedBusinessEvent
{
    using base_type = QnProlongedBusinessEvent;

public:
    QnSoftwareTriggerEvent(const QnResourcePtr& resource,
        const QString& triggerId, const QnUuid& userId, qint64 timeStampUs,
        QnBusiness::EventState toggleState = QnBusiness::UndefinedState);

    const QString& triggerId() const;

    const QnUuid& userId() const;

    virtual QnBusinessEventParameters getRuntimeParams() const override;
    virtual QnBusinessEventParameters getRuntimeParamsEx(
        const QnBusinessEventParameters& ruleEventParams) const override;

    virtual bool checkEventParams(const QnBusinessEventParameters &params) const override;

private:
    const QString m_triggerId;
    const QnUuid m_userId;
};

using QnSoftwareTriggerEventPtr = QSharedPointer<QnSoftwareTriggerEvent>;
