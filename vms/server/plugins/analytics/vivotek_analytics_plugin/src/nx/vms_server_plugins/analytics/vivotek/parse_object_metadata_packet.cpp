#include "parse_object_metadata_packet.h"

#include <stdexcept>
#include <algorithm>
#include <optional>
#include <cstring>

#include <nx/sdk/analytics/point.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>

#include "object_types.h"
#include "json_utils.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

std::optional<QString> parseTypeId(const QString& type)
{
    if (type == "Human")
        return kObjectTypeHuman;

    return std::nullopt;
}

Uuid parseTrackId(int id)
{
    Uuid uuid;
    std::memcpy(uuid.data(), &id, std::min(std::size_t(uuid.size()), sizeof(id)));
    return uuid;
}

Point parsePoint(const QString& path, const QJsonObject& point)
{
    auto parseCoord =
        [&](const QString& key)
        {
            // coordinates are normalized to [0; 10000]x[0; 10000]
            constexpr double kDomain = 10000;

            const auto value = get<double>(path, point, key);
            if (value < 0 || value > kDomain)
            {
                throw std::runtime_error(
                    NX_FMT("%1.%2 = %3 is outside of expected range of [0; 10000]",
                        path, key, value).toStdString());
            }

            return value / kDomain;
        };

    return Point(parseCoord("x"), parseCoord("y"));
}

Rect parseBoundingBox(const QString& path, const QJsonArray& pos2d)
{
    if (pos2d.isEmpty())
        throw std::runtime_error(NX_FMT("%1 array is empty", path).toStdString());

    Point min = {1, 1};
    Point max = {0, 0};
    for (int i = 0; i < pos2d.count(); ++i)
    {
        const auto point = parsePoint(NX_FMT("%1[%2]", path, i), get<QJsonObject>(path, pos2d, i));

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

Ptr<IObjectMetadata> parseMetadata(const QString& path, const QJsonObject& object)
{
    auto metadata = makePtr<ObjectMetadata>();

    if (auto typeId = parseTypeId(get<QString>(path, object, "Type")))
        metadata->setTypeId(typeId->toStdString());
    else
        return nullptr;

    metadata->setTrackId(parseTrackId(get<int>(path, object, "Id")));

    metadata->setBoundingBox(parseBoundingBox(
        NX_FMT("%1.Pos2D", path), get<QJsonArray>(path, object, "Pos2D")));

    metadata->setConfidence(1);

    return metadata;
}

} // namespace

Ptr<ObjectMetadataPacket> parseObjectMetadataPacket(const QJsonValue& native)
{
    if (get<QString>(native, "Tag") != "MetaData")
        return nullptr;

    if (!get<QJsonObject>(native, "Frame").contains("Objects"))
        return nullptr;

    auto packet = makePtr<ObjectMetadataPacket>();

    packet->setTimestampUs(parseIsoTimestamp(get<QString>(native, "Frame", "UtcTime")));

    const auto objects = get<QJsonArray>(native, "Frame", "Objects");
    for (int i = 0; i < objects.count(); ++i)
    {
        static const QString path = "$.Frame.Objects";
        if (const auto metadata = parseMetadata(
            NX_FMT("%1[%2]", path, i), get<QJsonObject>(path, objects, i)))
        {
            packet->addItem(metadata.get());
        }
    }

    return packet;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
