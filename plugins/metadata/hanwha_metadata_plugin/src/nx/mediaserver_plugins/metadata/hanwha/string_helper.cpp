#if defined(ENABLE_HANWHA)

#include "string_helper.h"

#include <nx/fusion/serialization/lexical_enum.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

QString StringHelper::buildCaption(
    const Hanwha::DriverManifest& manifest,
    const QnUuid& eventTypeId,
    boost::optional<int> /*eventChannel*/,
    boost::optional<int> /*eventRegion*/,
    Hanwha::EventItemType /*eventItemType*/,
    bool /*isActive*/)
{
    const auto& descriptor = manifest.eventDescriptorById(eventTypeId);
    return descriptor.name.value;
}

QString StringHelper::buildDescription(
    const Hanwha::DriverManifest& manifest,
    const QnUuid& eventTypeId,
    boost::optional<int> /*eventChannel*/,
    boost::optional<int> eventRegion,
    Hanwha::EventItemType eventItemType,
    bool isActive)
{
    const auto& descriptor = manifest.eventDescriptorById(eventTypeId);
    QString description = descriptor.description;
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
        const auto& regionStr = descriptor.regionDescription.arg(eventRegion ? *eventRegion : 0);
        description = description.arg(regionStr);
    }

    if (eventItemType != Hanwha::EventItemType::none)
        description = description.arg(QnLexical::serialized(eventItemType));

    return description;
}

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

#endif // defined(ENABLE_HANWHA)
