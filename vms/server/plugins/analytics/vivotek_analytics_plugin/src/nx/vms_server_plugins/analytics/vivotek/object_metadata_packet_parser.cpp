#include "object_metadata_packet_parser.h"

#include <chrono>
#include <algorithm>

#include <nx/utils/log/log_message.h>
#include <nx/vms_server_plugins/utils/exception.h>
#include <nx/sdk/analytics/point.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>

#include "camera_vca_parameter_api.h"
#include "object_types.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::vms_server_plugins::utils;

namespace {

constexpr auto kCacheEntryLifetime = 2min;

void expand(Rect* rect, const Point& point)
{
    Point min = {
        rect->x,
        rect->y,
    };
    Point max = {
        rect->x + rect->width,
        rect->y + rect->height,
    };

    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);

    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);

    rect->x = min.x;
    rect->y = min.y;
    rect->width = max.x - min.x;
    rect->height = max.y - min.y;
}

void expand(Rect* dest, const Rect& src)
{
    expand(dest, Point{src.x, src.y});
    expand(dest, Point{src.x + src.width, src.y + src.height});
}

} // namespace

Ptr<ObjectMetadataPacket> ObjectMetadataPacketParser::parse(const JsonValue& native)
{
    cleanupOldCacheEntries();

    if (native["Tag"].to<QString>() != "MetaData")
        return nullptr;

    if (!native["Frame"].to<JsonObject>().contains("Objects"))
        return nullptr;

    auto packet = makePtr<ObjectMetadataPacket>();

    packet->setTimestampUs(parseIsoTimestamp(native["Frame"]["UtcTime"].to<QString>()));
    packet->setDurationUs(1'000'000);

    const auto objects = native["Frame"]["Objects"].to<JsonArray>();
    for (int i = 0; i < objects.count(); ++i)
    {
        if (const auto metadata = parseMetadata(objects[i].to<JsonObject>()))
            packet->addItem(metadata.get());
    }

    return packet;
}

void ObjectMetadataPacketParser::cleanupOldCacheEntries()
{
    for (auto it = m_cache.begin(); it != m_cache.end(); )
    {
        const auto& entry = it->second;
        if (entry.timer.hasExpired(kCacheEntryLifetime))
            it = m_cache.erase(it);
        else
            ++it;
    }
}

std::optional<QString> ObjectMetadataPacketParser::parseTypeId(
    const QString& nativeId, const JsonArray& nativeBehaviors)
{
    for (const auto& type: kObjectTypes)
    {
        if (type.nativeId != nativeId)
            continue;

        if (type.nativeBehavior && !nativeBehaviors.contains(*type.nativeBehavior))
            continue;

        return type.id;
    }

    return std::nullopt;
}

Uuid ObjectMetadataPacketParser::parseTrackId(int nativeId)
{
    auto& entry = m_cache[nativeId];

    entry.timer.restart();

    return entry.trackId;
}

Rect ObjectMetadataPacketParser::parsePolygonBoundingBox(const JsonArray& pos2d)
{
    if (pos2d.isEmpty())
        throw Exception("%1 array is empty", pos2d.path);

    Rect rect;
    rect.x = 1;
    rect.y = 1;
    for (int i = 0; i < pos2d.count(); ++i)
        expand(&rect, CameraVcaParameterApi::parsePoint(pos2d[i].to<JsonObject>()));

    return rect;
}

Rect ObjectMetadataPacketParser::parseRectangleBoundingBox(const JsonObject& rectangle)
{
    return {
        CameraVcaParameterApi::parseCoordinate(rectangle["x"]),
        CameraVcaParameterApi::parseCoordinate(rectangle["y"]),
        CameraVcaParameterApi::parseCoordinate(rectangle["w"]),
        CameraVcaParameterApi::parseCoordinate(rectangle["h"]),
    };
}

Ptr<IObjectMetadata> ObjectMetadataPacketParser::parseMetadata(const JsonObject& object)
{
    auto metadata = makePtr<ObjectMetadata>();

    if (const auto typeId = parseTypeId(
            object["Type"].to<QString>(), object["Behaviour"].to<JsonArray>()))
    {
        metadata->setTypeId(typeId->toStdString());
    }
    else
    {
        return nullptr;
    }

    metadata->setTrackId(parseTrackId(object["Id"].to<int>()));

    Rect rect;
    rect.x = 1;
    rect.y = 1;
    if (const auto pos2d = object["Pos2D"]; pos2d.isArray())
        expand(&rect, parsePolygonBoundingBox(pos2d.to<JsonArray>()));
    if (const auto face = object["Face"]; face.isObject())
        expand(&rect, parseRectangleBoundingBox(face["Rectangle"].to<JsonObject>()));
    if (rect.width < 0 && rect.height < 0)
        return nullptr;
    metadata->setBoundingBox(rect);

    return metadata;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
