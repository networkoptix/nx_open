#include "parse_object_metadata_packet.h"

#include <algorithm>
#include <optional>
#include <cstring>

#include <nx/sdk/analytics/point.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/utils/log/log_message.h>

#include "camera_vca_parameter_api.h"
#include "object_types.h"
#include "json_utils.h"
#include "exception.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

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

Rect parseBoundingBox(const QString& path, const QJsonArray& pos2d)
{
    if (pos2d.isEmpty())
        throw Exception("%1 array is empty", path);

    Point min = {1, 1};
    Point max = {0, 0};
    for (int i = 0; i < pos2d.count(); ++i)
    {
        const auto point = CameraVcaParameterApi::parsePoint(
            get<QJsonObject>(path, pos2d, i), NX_FMT("%1[%2]", path, i));

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

    return metadata;
}

} // namespace

Ptr<IObjectMetadataPacket> parseObjectMetadataPacket(const QJsonValue& native)
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
