// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_type.h"

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

EventType::EventType(
    EventTypeDescriptor eventTypeDescriptor,
    AbstractResourceSupportProxy* resourceSupportProxy)
    :
    base_type(
        EntityType::eventType,
        eventTypeDescriptor,
        kEventTypeDescriptorTypeName,
        resourceSupportProxy)
{
}

bool EventType::isStateDependent() const
{
    return m_descriptor.flags.testFlag(EventTypeFlag::stateDependent);
}

bool EventType::isRegionDependent() const
{
    return m_descriptor.flags.testFlag(EventTypeFlag::regionDependent);
}

bool EventType::isHidden() const
{
    return m_descriptor.flags.testFlag(EventTypeFlag::hidden);
}

bool EventType::useTrackBestShotAsPreview() const
{
    return m_descriptor.flags.testFlag(EventTypeFlag::useTrackBestShotAsPreview);
}

} // namespace nx::analytics::taxonomy
