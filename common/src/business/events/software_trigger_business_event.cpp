#include "software_trigger_business_event.h"


QnSoftwareTriggerEvent::QnSoftwareTriggerEvent(
    const QnResourcePtr& resource,
    const QString& triggerId,
    qint64 timeStampUs,
    QnBusiness::EventState toggleState)
    :
    base_type(QnBusiness::SoftwareTriggerEvent, resource, toggleState, timeStampUs),
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

QnBusinessEventParameters QnSoftwareTriggerEvent::getRuntimeParamsEx(
    const QnBusinessEventParameters& ruleEventParams) const
{
    auto params = getRuntimeParams();
    params.caption = ruleEventParams.caption;
    params.description = ruleEventParams.description;
    return params;
}

bool QnSoftwareTriggerEvent::checkEventParams(const QnBusinessEventParameters& params) const
{
    return m_triggerId == params.inputPortId;
}
