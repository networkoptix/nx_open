#include "string_helper.h"

#include <nx/fusion/serialization/lexical_enum.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

QString buildCaption(
    const Hikvision::EngineManifest& manifest,
    const HikvisionEvent& event)
{
    const auto descriptor = manifest.eventTypeDescriptorById(event.typeId);
    return descriptor.name;
}

QString buildDescription(
    const Hikvision::EngineManifest& manifest,
    const HikvisionEvent& event)
{
    using namespace nx::vms::api::analytics;

    const auto descriptor = manifest.eventTypeDescriptorById(event.typeId);
    auto description = descriptor.description;
    if (description.isEmpty())
        return QString();

    if (descriptor.flags.testFlag(EventTypeFlag::stateDependent))
    {
        auto stateStr = event.isActive ? descriptor.positiveState : descriptor.negativeState;
        if (!stateStr.isEmpty())
            description = description.arg(stateStr);
    }

    if (descriptor.flags.testFlag(EventTypeFlag::regionDependent))
    {
        auto regionStr = descriptor.regionDescription.arg(event.region ? *event.region : 0);
        description = description.arg(regionStr);
    }

    return description;
}

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
