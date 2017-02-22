#pragma once

#include <business/events/abstract_business_event.h>


class QnSoftwareTriggerEvent: public QnAbstractBusinessEvent
{
    using base_type = QnAbstractBusinessEvent;

public:
    QnSoftwareTriggerEvent(const QnResourcePtr& resource, qint64 timeStampUs, const QString& triggerId);

    const QString& triggerId() const;

    virtual QnBusinessEventParameters getRuntimeParams() const override;
    virtual bool checkEventParams(const QnBusinessEventParameters &params) const override;
    virtual bool isEventStateMatched(QnBusiness::EventState state,
        QnBusiness::ActionType actionType) const override;

private:
    const QString m_triggerId;
};

typedef QSharedPointer<QnSoftwareTriggerEvent> QnSoftwareTriggerEventPtr;
