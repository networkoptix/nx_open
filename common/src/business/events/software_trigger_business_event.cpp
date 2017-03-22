#include "software_trigger_business_event.h"


QnSoftwareTriggerEvent::QnSoftwareTriggerEvent(const QnResourcePtr& resource, qint64 timeStampUs, const QString& triggerId):
    base_type(QnBusiness::SoftwareTriggerEvent, resource, QnBusiness::UndefinedState, timeStampUs),
    m_triggerId(triggerId)
{
}

const QString& QnSoftwareTriggerEvent::triggerId() const
{
    return m_triggerId;
}

QnBusinessEventParameters QnSoftwareTriggerEvent::getRuntimeParams() const
{
    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.inputPortId = m_triggerId;
    return params;
}

bool QnSoftwareTriggerEvent::checkEventParams(const QnBusinessEventParameters& params) const
{
    return m_triggerId == params.inputPortId;
}

bool QnSoftwareTriggerEvent::isEventStateMatched(QnBusiness::EventState state,
    QnBusiness::ActionType /*actionType*/) const
{
    return state == QnBusiness::UndefinedState;
}
