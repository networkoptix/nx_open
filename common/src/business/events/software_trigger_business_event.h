#pragma once

#include <business/events/prolonged_business_event.h>


class QnSoftwareTriggerEvent: public QnProlongedBusinessEvent
{
    using base_type = QnProlongedBusinessEvent;

public:
    QnSoftwareTriggerEvent(const QnResourcePtr& resource, const QString& triggerId,
        qint64 timeStampUs, QnBusiness::EventState toggleState = QnBusiness::UndefinedState);

    const QString& triggerId() const;

    virtual QnBusinessEventParameters getRuntimeParams() const override;
    virtual QnBusinessEventParameters getRuntimeParamsEx(
        const QnBusinessEventParameters& ruleEventParams) const override;

    virtual bool checkEventParams(const QnBusinessEventParameters &params) const override;

private:
    const QString m_triggerId;
};

using QnSoftwareTriggerEventPtr = QSharedPointer<QnSoftwareTriggerEvent>;
