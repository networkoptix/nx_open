#include "software_trigger_business_event.h"

#include <nx/utils/uuid.h>

QnSoftwareTriggerEvent::QnSoftwareTriggerEvent(
    const QnResourcePtr& resource,
    const QString& triggerId,
    const QnUuid& userId,
    qint64 timeStampUs,
    QnBusiness::EventState toggleState)
    :
    base_type(QnBusiness::SoftwareTriggerEvent, resource, toggleState, timeStampUs),
    m_triggerId(triggerId),
    m_userId(userId)
{
}

const QString& QnSoftwareTriggerEvent::triggerId() const
{
    return m_triggerId;
}

const QnUuid& QnSoftwareTriggerEvent::userId() const
{
    return m_userId;
}

QnBusinessEventParameters QnSoftwareTriggerEvent::getRuntimeParams() const
{
    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.inputPortId = m_triggerId;
    params.metadata.instigators = { m_userId };
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
