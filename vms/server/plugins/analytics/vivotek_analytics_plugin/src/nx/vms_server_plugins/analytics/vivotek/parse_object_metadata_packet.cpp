#include "parse_object_metadata_packet.h"

#include <algorithm>
#include <optional>
#include <cstring>

#include <nx/sdk/analytics/point.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/utils/log/log_message.h>

#include "camera_vca_parameter_api.h"
#include "object_types.h"
#include "exception.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

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

std::optional<QString> parseTypeId(const QString& native)
{
    for (const auto& type: kObjectTypes)
    {
        if (type.nativeId == native)
            return type.id;
    }

    return std::nullopt;
}

Uuid parseTrackId(int id)
{
    Uuid uuid;
    std::memcpy(uuid.data(), &id, std::min(std::size_t(uuid.size()), sizeof(id)));
    return uuid;
}

Rect parsePolygonBoundingBox(const JsonArray& pos2d)
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

Rect parseRectangleBoundingBox(const JsonObject& rectangle)
{
    return {
        CameraVcaParameterApi::parseCoordinate(rectangle["x"]),
        CameraVcaParameterApi::parseCoordinate(rectangle["y"]),
        CameraVcaParameterApi::parseCoordinate(rectangle["w"]),
        CameraVcaParameterApi::parseCoordinate(rectangle["h"]),
    };
}

Ptr<IObjectMetadata> parseMetadata(const JsonObject& object)
{
    auto metadata = makePtr<ObjectMetadata>();

    if (const auto typeId = parseTypeId(object["Type"].to<QString>()))
        metadata->setTypeId(typeId->toStdString());
    else
        return nullptr;

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

} // namespace

Ptr<ObjectMetadataPacket> parseObjectMetadataPacket(const JsonValue& native)
{
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

} // namespace nx::vms_server_plugins::analytics::vivotek
