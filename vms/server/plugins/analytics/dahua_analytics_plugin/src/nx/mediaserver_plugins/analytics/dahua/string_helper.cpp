#include "string_helper.h"

#include <nx/utils/literal.h>
#include <nx/utils/std/hashes.h>

#include <plugins/plugin_tools.h>
#include <nx/fusion/serialization/lexical_enum.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace dahua {

QString buildCaption(const Dahua::EngineManifest& manifest, const DahuaEvent& event)
{
    const auto descriptor = manifest.eventTypeDescriptorById(event.typeId);
    return descriptor.name.value;
}

QString buildDescription(const Dahua::EngineManifest& manifest, const DahuaEvent& event)
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
} // namespace mediaserver_plugins
} // namespace nx
