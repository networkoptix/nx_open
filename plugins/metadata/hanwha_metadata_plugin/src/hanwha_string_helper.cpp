#include "hanwha_string_helper.h"
#include "hanwha_common.h"

#include <nx/utils/literal.h>
#include <nx/utils/std/hashes.h>

#include <plugins/plugin_tools.h>
#include <nx/fusion/serialization/lexical_enum.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace mediaserver {
namespace plugins {

QString HanwhaStringHelper::buildCaption(
    const Hanwha::DriverManifest& manifest,
    const QnUuid& eventTypeId,
    boost::optional<int> eventChannel,
    boost::optional<int> eventRegion,
    Hanwha::EventItemType eventItemType,
    bool isActive)
{
    const auto descriptor = manifest.eventDescriptorById(eventTypeId);
    return descriptor.eventName.value;
}

QString HanwhaStringHelper::buildDescription(
    const Hanwha::DriverManifest& manifest,
    const QnUuid& eventTypeId,
    boost::optional<int> eventChannel,
    boost::optional<int> eventRegion,
    Hanwha::EventItemType eventItemType,
    bool isActive)
{
    const auto descriptor = manifest.eventDescriptorById(eventTypeId);
    auto description = descriptor.description;
    if (description.isEmpty())
        return QString();

    if (descriptor.flags.testFlag(Hanwha::EventTypeFlag::stateDependent))
    {
        auto stateStr = isActive ? descriptor.positiveState : descriptor.negativeState;
        if (!stateStr.isEmpty())
            description = description.arg(stateStr);
    }

    if (descriptor.flags.testFlag(Hanwha::EventTypeFlag::regionDependent))
    {
        auto regionStr = descriptor.regionDescription.arg(eventRegion ? *eventRegion : 0);
        description = description.arg(regionStr);
    }

    if (eventItemType != Hanwha::EventItemType::none)
        description = description.arg(QnLexical::serialized(eventItemType));

    return description;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
