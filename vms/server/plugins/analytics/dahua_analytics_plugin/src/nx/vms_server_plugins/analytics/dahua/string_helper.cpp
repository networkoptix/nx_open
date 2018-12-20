#include <nx/fusion/serialization/lexical_enum.h>
#include <plugins/plugin_tools.h>

#include <nx/utils/literal.h>
#include <nx/utils/std/hashes.h>
#include <nx/utils/thread/mutex.h>

#include "string_helper.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dahua {

QString buildCaption(const EngineManifest& manifest, const Event& event)
{
    const auto descriptor = manifest.eventTypeDescriptorById(event.typeId);
    return descriptor.name;
}

QString buildDescription(const EngineManifest& manifest, const Event& event)
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

} // namespace dahua
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
