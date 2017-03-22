#pragma once

#include <business/events/abstract_business_event.h>


class QnSoftwareTriggerEvent: public QnAbstractBusinessEvent
{
    using base_type = QnAbstractBusinessEvent;

public:
    QnSoftwareTriggerEvent(const QnResourcePtr& resource, qint64 timeStampUs,
        const QString& triggerId, const QString& triggerName, const QString& iconName);

    const QString& triggerId() const;
    const QString& triggerName() const;
    const QString& iconName() const;

    virtual QnBusinessEventParameters getRuntimeParams() const override;
    virtual bool checkEventParams(const QnBusinessEventParameters &params) const override;
    virtual bool isEventStateMatched(QnBusiness::EventState state,
        QnBusiness::ActionType actionType) const override;

private:
    const QString m_triggerId;
    const QString m_triggerName;
    const QString m_iconName;
};

using QnSoftwareTriggerEventPtr = QSharedPointer<QnSoftwareTriggerEvent>;
