#include "string_helper.h"

#include <nx/fusion/serialization/lexical_enum.h>
#include <nx/utils/log/log_main.h>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

QString StringHelper::buildCaption(
    const Hanwha::EngineManifest& /*manifest*/,
    const QString& /*eventTypeId*/,
    std::optional<int> /*eventChannel*/,
    std::optional<int> /*eventRegion*/,
    Hanwha::EventItemType /*eventItemType*/,
    bool /*isActive*/)
{
    return QString(); //< Not used so far.
}

QString StringHelper::buildDescription(
    const Hanwha::EngineManifest& manifest,
    const QString& eventTypeId,
    std::optional<int> /*eventChannel*/,
    std::optional<int> eventRegion,
    Hanwha::EventItemType eventItemType,
    bool isActive)
{
    using namespace nx::vms::api::analytics;

    const auto& descriptor = manifest.eventTypeDescriptorById(eventTypeId);
    QString description = descriptor.description;
    if (description.isEmpty())
        return QString();

    if (descriptor.flags.testFlag(EventTypeFlag::stateDependent))
    {
        auto stateStr = isActive ? descriptor.positiveState : descriptor.negativeState;
        if (!stateStr.isEmpty())
            description = description.arg(stateStr);
    }

    if (descriptor.flags.testFlag(EventTypeFlag::regionDependent))
    {
        const auto regionStr =
            eventRegion ? descriptor.regionDescription.arg(*eventRegion) : QString();
        description = description.arg(regionStr);
    }

    if (eventItemType != Hanwha::EventItemType::none)
        description = description.arg(QnLexical::serialized(eventItemType));

    return description;
}

} // namespace hanwha
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
