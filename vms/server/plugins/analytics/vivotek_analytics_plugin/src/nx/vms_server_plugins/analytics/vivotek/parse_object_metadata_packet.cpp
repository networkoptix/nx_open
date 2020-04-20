#include "parse_object_metadata_packet.h"

#include <stdexcept>
#include <algorithm>
#include <optional>
#include <string>
#include <cstring>

#include <nx/sdk/analytics/point.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>

#include "object_types.h"
#include "parse_iso_timestamp.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {
    // Screen coordinates in Pos2D are always scaled to [0; 10000]x[0; 10000]
    constexpr int kCoordDomain = 10000;

    std::optional<std::string> parseType(std::string_view type)
    {
        if (type == "Human")
            return kObjectTypeHuman;

        return std::nullopt;
    }

    Uuid bitCastToUuid(int value)
    {
        Uuid uuid;
        std::memcpy(uuid.data(), &value, std::min((std::size_t) uuid.size(), sizeof(value)));
        return uuid;
    }

    float parseCoord(std::size_t objectIndex, std::size_t pointIndex, const std::string& coordKey,
        const Json& coord)
    {
        if (!coord.is_number())
            throw std::runtime_error("$.Frame.Objects[" + std::to_string(objectIndex)
                + "].Pos2D[" + std::to_string(pointIndex)
                + "]." + coordKey + " in metadata json is not a number");
        double value = coord.number_value();
        if (value < 0 || kCoordDomain < value)
            throw std::runtime_error("$.Frame.Objects[" + std::to_string(objectIndex)
                + "].Pos2D[" + std::to_string(pointIndex)
                + "]." + coordKey + " number in metadata json is out of expected range of "
                + "[0; " + std::to_string(kCoordDomain) + "]");

        return value / kCoordDomain;
    }

    Point parsePoint(std::size_t objectIndex, std::size_t pointIndex, const Json& point)
    {
        if (!point.is_object())
            throw std::runtime_error("$.Frame.Objects[" + std::to_string(objectIndex)
                + "].Pos2D[" + std::to_string(pointIndex)
                + "] in metadata json is not an object");

        return Point(
            parseCoord(objectIndex, pointIndex, "x", point["x"]),
            parseCoord(objectIndex, pointIndex, "y", point["y"]));
    }

    Rect parseBoundingBox(std::size_t objectIndex, const Json& pos2d)
    {
        if (!pos2d.is_array())
            throw std::runtime_error("$.Frame.Objects[" + std::to_string(objectIndex)
                + "].Pos2D in metadata json is not an array");
        const auto& pos2dArray = pos2d.array_items();
        if (pos2dArray.empty())
            throw std::runtime_error("$.Frame.Objects[" + std::to_string(objectIndex)
                + "].Pos2D array in metadata json is empty");

        Point min = {1, 1};
        Point max = {0, 0};
        for (std::size_t i = 0; i < pos2dArray.size(); ++i)
        {
            const auto point = parsePoint(objectIndex, i, pos2dArray[i]);

            min.x = std::min(min.x, point.x);
            min.y = std::min(min.y, point.y);

            max.x = std::max(max.x, point.x);
            max.y = std::max(max.y, point.y);
        }

        Rect rect;
        rect.x = min.x;
        rect.y = min.y;
        rect.width = max.x - min.x;
        rect.height = max.y - min.y;
        return rect;
    }

    Ptr<IObjectMetadata> parseMetadata(std::size_t objectIndex, const Json& object)
    {
        if (!object.is_object())
            throw std::runtime_error("$.Frame.Objects[" + std::to_string(objectIndex)
                + "] in metadata json is not an object");

        const auto metadata = makePtr<ObjectMetadata>();

        const auto& type = object["Type"];
        if (!type.is_string())
            throw std::runtime_error("$.Frame.Objects[" + std::to_string(objectIndex)
                + "].Type in metadata json is not a string");
        if (auto typeId = parseType(type.string_value()))
            metadata->setTypeId(*typeId);
        else
            return nullptr;

        const auto& id = object["Id"];
        if (!id.is_number())
            throw std::runtime_error("$.Frame.Objects[" + std::to_string(objectIndex)
                + "].Id in metadata json is not a number");
        metadata->setTrackId(bitCastToUuid(id.int_value()));

        metadata->setBoundingBox(parseBoundingBox(objectIndex, object["Pos2D"]));

        metadata->setConfidence(1);

        return metadata;
    }
} // namespace

Ptr<IObjectMetadataPacket> parseObjectMetadataPacket(Json const& nativeMetadata)
{
    const auto& tag = nativeMetadata["Tag"];
    if (!tag.is_string())
        throw std::runtime_error("$.Tag in metadata json is not a string");
    if (tag.string_value() != "MetaData")
        return nullptr;

    const auto& frame = nativeMetadata["Frame"];
    if (!frame.is_object())
        throw std::runtime_error("$.Frame in metadata json is not an object");

    auto packet = makePtr<ObjectMetadataPacket>();

    const auto& utcTime = frame["UtcTime"];
    if (!utcTime.is_string())
        throw std::runtime_error("$.Frame.UtcTime in metadata json is not a string");
    packet->setTimestampUs(parseIsoTimestamp(utcTime.string_value()));

    if (!frame.object_items().count("Objects"))
        return nullptr;
    const auto& objects = frame["Objects"];
    if (!objects.is_array())
        throw std::runtime_error("$.Frame.Objects in metadata json is not an array");
    const auto& objectArray = objects.array_items();
    for (std::size_t i = 0; i < objectArray.size(); ++i)
        if (const auto metadata = parseMetadata(i, objectArray[i]))
            packet->addItem(metadata.get());

    return packet;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
