#include "parse_event_metadata_packets.h"

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>

#include "event_types.h"
#include "utils.h"
#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

std::optional<QString> parseTypeId(const QString& native)
{
    for (const auto& type: kEventTypes)
    {
        if (type.nativeId == native)
            return type.id;
    }

    return std::nullopt;
}

Ptr<IEventMetadataPacket> parsePacket(const JsonObject& behaviorAlarmInfo)
{
    auto packet = makePtr<EventMetadataPacket>();

    packet->setTimestampUs(parseIsoTimestamp(behaviorAlarmInfo["Time"].to<QString>()));

    auto metadata = makePtr<EventMetadata>();

    if (auto typeId = parseTypeId(behaviorAlarmInfo["RuleType"].to<QString>()))
        metadata->setTypeId(typeId->toStdString());
    else
        return nullptr;

    metadata->setDescription(behaviorAlarmInfo["RuleName"].to<QString>().toStdString());

    packet->addItem(metadata.get());

    return packet;
}

} // namespace

std::vector<Ptr<IEventMetadataPacket>> parseEventMetadataPackets(const JsonValue& native)
{
    std::vector<Ptr<IEventMetadataPacket>> packets;

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
