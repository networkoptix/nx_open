#pragma once

#include <optional>

#include <nx/utils/elapsed_timer.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/uuid.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

#include <QtCore/QString>

#include "json_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class ObjectMetadataPacketParser
{
public:
    nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> parse(const JsonValue& native);

private:
    struct CacheEntry
    {
        nx::utils::ElapsedTimer timer;
        
        const nx::sdk::Uuid trackId = nx::sdk::UuidHelper::randomUuid();
    };

private:
    void cleanupOldCacheEntries();

    std::optional<QString> parseTypeId(const QString& nativeId, const JsonArray& nativeBehaviors);
    nx::sdk::Uuid parseTrackId(int id);
    nx::sdk::analytics::Rect parsePolygonBoundingBox(const JsonArray& pos2d);
    nx::sdk::analytics::Rect parseRectangleBoundingBox(const JsonObject& rectangle);
    nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadata> parseMetadata(const JsonObject& object);

private:
    std::map<int, CacheEntry> m_cache;
};


} // namespace nx::vms_server_plugins::analytics::vivotek
