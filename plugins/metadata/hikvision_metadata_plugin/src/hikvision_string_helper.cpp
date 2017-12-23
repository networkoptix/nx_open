#include "hikvision_string_helper.h"

#include <nx/utils/literal.h>
#include <nx/utils/std/hashes.h>

#include <plugins/plugin_tools.h>
#include <nx/fusion/serialization/lexical_enum.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace mediaserver {
namespace plugins {

QString HikvisionStringHelper::buildCaption(
    const Hikvision::DriverManifest& manifest,
    const QnUuid& eventTypeId,
    boost::optional<int> eventChannel,
    boost::optional<int> eventRegion,
    bool isActive)
{
    const auto descriptor = manifest.eventDescriptorById(eventTypeId);
    return descriptor.eventName.value;
}

QString HikvisionStringHelper::buildDescription(
    const Hikvision::DriverManifest& manifest,
    const QnUuid& eventTypeId,
    boost::optional<int> eventChannel,
    boost::optional<int> eventRegion,
    bool isActive)
{
    const auto descriptor = manifest.eventDescriptorById(eventTypeId);
    auto description = descriptor.description;
    if (description.isEmpty())
        return QString();

    if (descriptor.flags.testFlag(Hikvision::EventTypeFlag::stateDependent))
    {
        auto stateStr = isActive ? descriptor.positiveState : descriptor.negativeState;
        if (!stateStr.isEmpty())
            description = description.arg(stateStr);
    }

    if (descriptor.flags.testFlag(Hikvision::EventTypeFlag::regionDependent))
    {
        auto regionStr = descriptor.regionDescription.arg(eventRegion ? *eventRegion : 0);
        description = description.arg(regionStr);
    }

    return description;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
