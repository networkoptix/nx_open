#include "parse_event_metadata_packets.h"

#include <nx/vms_server_plugins/utils/exception.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>

#include "event_types.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::vms_server_plugins::utils;

namespace {

const EventType* findEventType(const QString& native)
{
    for (const auto& type: kEventTypes)
    {
        if (type.nativeId == native)
            return &type;
    }

    return nullptr;
}

Ptr<EventMetadataPacket> parsePacket(const JsonObject& behaviorAlarmInfo)
{
    auto packet = makePtr<EventMetadataPacket>();

    packet->setTimestampUs(parseIsoTimestamp(behaviorAlarmInfo["Time"].to<QString>()));

    auto metadata = makePtr<EventMetadata>();

    const auto* type = findEventType(behaviorAlarmInfo["RuleType"].to<QString>());
    if (!type)
        return nullptr;

    metadata->setTypeId(type->id.toStdString());

    const auto edgeEvent = behaviorAlarmInfo["EdgeEvent"].to<QString>();
    if (!type->isProlonged && edgeEvent != "Rising")
        return nullptr;

    metadata->setIsActive(edgeEvent != "Falling");

    auto description = behaviorAlarmInfo["RuleName"].to<QString>();
    if (type->isProlonged)
    {
        if (edgeEvent == "Rising")
            description += " has started";
        else if (edgeEvent == "Falling")
            description += " has ended";
    }
    metadata->setDescription(description.toStdString());

    packet->addItem(metadata.get());

    return packet;
}

} // namespace

std::vector<Ptr<EventMetadataPacket>> parseEventMetadataPackets(const JsonValue& native)
{
    std::vector<Ptr<EventMetadataPacket>> packets;

    if (native["Tag"].to<QString>() != "Event")
        return packets;

    const auto data = native["Data"].to<JsonArray>();
    for (int i = 0; i < data.size(); ++i)
    {
        const auto behaviorAlarmInfo = data[i]["BehaviorAlarmInfo"].to<JsonArray>();
        for (int j = 0; j < behaviorAlarmInfo.size(); ++j)
        {
            if (auto packet = parsePacket(behaviorAlarmInfo[j].to<JsonObject>()))
                packets.push_back(std::move(packet));
        }
    }

    return packets;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
