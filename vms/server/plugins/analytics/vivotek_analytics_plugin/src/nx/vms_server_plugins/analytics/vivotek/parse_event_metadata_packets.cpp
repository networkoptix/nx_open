#include "parse_event_metadata_packets.h"

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>

#include "event_types.h"
#include "json_utils.h"
#include "utils.h"
#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

std::optional<QString> parseTypeId(const QString& type)
{
    if (type == "CrowdDetection")
        return kEventTypeCrowd;
    if (type == "LoiteringDetection")
        return kEventTypeLoitering;
    if (type == "IntrusionDetection")
        return kEventTypeIntrusion;

    return std::nullopt;
}

Ptr<IEventMetadataPacket> parsePacket(const QString& path, const QJsonObject& behaviorAlarmInfo)
{
    auto packet = makePtr<EventMetadataPacket>();

    packet->setTimestampUs(parseIsoTimestamp(get<QString>(path, behaviorAlarmInfo, "Time")));

    auto metadata = makePtr<EventMetadata>();

    if (auto typeId = parseTypeId(get<QString>(path, behaviorAlarmInfo, "RuleType")))
        metadata->setTypeId(typeId->toStdString());
    else
        return nullptr;

    metadata->setDescription(get<QString>(path, behaviorAlarmInfo, "RuleName").toStdString());

    packet->addItem(metadata.get());

    return packet;
}

} // namespace

std::vector<Ptr<IEventMetadataPacket>> parseEventMetadataPackets(const QJsonValue& native)
{
    std::vector<Ptr<IEventMetadataPacket>> packets;

    if (get<QString>(native, "Tag") != "Event")
        return packets;

    const auto data = get<QJsonArray>(native, "Data");
    for (int i = 0; i < data.size(); ++i)
    {
        const QString path = NX_FMT("$.Data[%1]", i);
        const auto behaviorAlarmInfo = get<QJsonArray>(path, data[i], "BehaviorAlarmInfo");
        for (int j = 0; j < behaviorAlarmInfo.size(); ++j)
        {
            const QString subPath = NX_FMT("%1.BehaviorAlarmInfo", path);
            if (auto packet = parsePacket(
                NX_FMT("%1[%2]", subPath, j), get<QJsonObject>(subPath, behaviorAlarmInfo, j)))
            {
                packets.push_back(std::move(packet));
            }
        }
    }

    return packets;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
