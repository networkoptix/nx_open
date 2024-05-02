// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_event.h"

namespace nx {
namespace vms {
namespace event {

CustomEvent::CustomEvent(
    const nx::vms::event::EventParameters& parameters,
    nx::vms::api::EventState eventState,
    const QnResourcePtr& resource)
    :
    base_type(EventType::userDefinedEvent, resource, eventState, parameters.eventTimestampUsec),
    m_resourceName(parameters.resourceName),
    m_caption(parameters.caption),
    m_description(parameters.description),
    m_metadata(parameters.metadata)
{
}

bool CustomEvent::checkEventParams(const EventParameters& params) const
{
    return checkForKeywords(m_resourceName, params.resourceName)
        && checkForKeywords(m_caption, params.caption)
        && checkForKeywords(m_description, params.description);
}

EventParameters CustomEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.resourceName = m_resourceName;
    params.caption = m_caption;
    params.description = m_description;
    params.metadata = m_metadata;
    return params;
}

} // namespace event
} // namespace vms
} // namespace nx
