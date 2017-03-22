#include "software_trigger_business_event.h"


QnSoftwareTriggerEvent::QnSoftwareTriggerEvent(const QnResourcePtr& resource, qint64 timeStampUs,
    const QString& triggerId, const QString& triggerName, const QString& iconName)
    :
    base_type(QnBusiness::SoftwareTriggerEvent, resource, QnBusiness::UndefinedState, timeStampUs),
    m_triggerId(triggerId),
    m_triggerName(triggerName),
    m_iconName(iconName)
{
}

const QString& QnSoftwareTriggerEvent::triggerId() const
{
    return m_triggerId;
}

const QString& QnSoftwareTriggerEvent::triggerName() const
{
    return m_triggerName;
}

const QString& QnSoftwareTriggerEvent::iconName() const
{
    return m_iconName;
}

QnBusinessEventParameters QnSoftwareTriggerEvent::getRuntimeParams() const
{
    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.inputPortId = m_triggerId;
    params.caption = m_triggerName;
    params.description = m_iconName;
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
