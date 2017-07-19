#include "software_trigger_event.h"

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace event {

SoftwareTriggerEvent::SoftwareTriggerEvent(
    const QnResourcePtr& resource,
    const QString& triggerId,
    const QnUuid& userId,
    qint64 timeStampUs,
    EventState toggleState)
    :
    base_type(softwareTriggerEvent, resource, toggleState, timeStampUs),
    m_triggerId(triggerId),
    m_userId(userId)
{
}

const QString& SoftwareTriggerEvent::triggerId() const
{
    return m_triggerId;
}

const QnUuid& SoftwareTriggerEvent::userId() const
{
    return m_userId;
}

EventParameters SoftwareTriggerEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.inputPortId = m_triggerId;
    params.metadata.instigators = { m_userId };
    return params;
}

EventParameters SoftwareTriggerEvent::getRuntimeParamsEx(
    const EventParameters& ruleEventParams) const
{
    auto params = getRuntimeParams();
    params.caption = ruleEventParams.caption;
    params.description = ruleEventParams.description;
    return params;
}

bool SoftwareTriggerEvent::checkEventParams(const EventParameters& params) const
{
    return m_triggerId == params.inputPortId;
}

} // namespace event
} // namespace vms
} // namespace nx
