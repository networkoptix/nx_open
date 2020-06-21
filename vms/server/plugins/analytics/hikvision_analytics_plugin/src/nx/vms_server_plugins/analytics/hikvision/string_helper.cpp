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
    const auto eventType = manifest.eventTypeById(event.typeId);
    return eventType.name;
}

QString buildDescription(
    const Hikvision::EngineManifest& manifest,
    const HikvisionEvent& event)
{
    using namespace nx::vms::api::analytics;

    const auto eventType = manifest.eventTypeById(event.typeId);
    auto description = eventType.description;
    if (description.isEmpty())
        return QString();

    if (eventType.flags.testFlag(EventTypeFlag::stateDependent))
    {
        auto stateStr = event.isActive ? eventType.positiveState : eventType.negativeState;
        if (!stateStr.isEmpty())
            description = description.arg(stateStr);
    }

    if (eventType.flags.testFlag(EventTypeFlag::regionDependent))
    {
        auto regionStr = eventType.regionDescription.arg(event.region ? event.region->id : 0);
        description = description.arg(regionStr);
    }

    return description;
}

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
