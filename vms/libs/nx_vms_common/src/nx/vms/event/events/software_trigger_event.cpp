// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_trigger_event.h"

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace event {

SoftwareTriggerEvent::SoftwareTriggerEvent(
    const QnResourcePtr& resource,
    const QString& triggerId,
    const nx::Uuid& userId,
    qint64 timeStampUs,
    EventState toggleState,
    const QString& triggerName,
    const QString& triggerIcon)
    :
    base_type(EventType::softwareTriggerEvent, resource, toggleState, timeStampUs),
    m_triggerId(triggerId),
    m_userId(userId),
    m_triggerName(triggerName),
    m_triggerIcon(triggerIcon)
{
}

const QString& SoftwareTriggerEvent::triggerId() const
{
    return m_triggerId;
}

const nx::Uuid& SoftwareTriggerEvent::userId() const
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

const QString& SoftwareTriggerEvent::triggerName() const
{
    return m_triggerName;
}

const QString& SoftwareTriggerEvent::triggerIcon() const
{
    return m_triggerIcon;
}

} // namespace event
} // namespace vms
} // namespace nx
