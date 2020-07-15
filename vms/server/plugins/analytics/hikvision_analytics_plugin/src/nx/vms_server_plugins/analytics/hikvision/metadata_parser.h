#pragma once

#include <chrono>
#include <optional>
#include <map>
#include <vector>

#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/analytics/point.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/analytics/i_object_metadata.h>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtXml/QDomElement>

namespace nx::vms_server_plugins::analytics::hikvision {

class MetadataParser
{
public:
    nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket> parsePacket(QByteArray bytes);

private:
    struct CacheEntry
    {
        std::chrono::steady_clock::time_point lastUpdate;
        nx::sdk::analytics::Rect boundingBox;
        std::vector<nx::sdk::Ptr<nx::sdk::Attribute>> attributes;
    };

private:
    std::optional<nx::sdk::Uuid> parseTrackId(const QString& stringId) const;

    QString parseTypeId(const QString& recognition) const;

    std::optional<nx::sdk::analytics::Point> parsePoint(const QDomElement& point) const;
    std::optional<nx::sdk::analytics::Rect> parseBoundingBox(const QDomElement& regionList) const;
    
    std::optional<std::vector<nx::sdk::Ptr<nx::sdk::Attribute>>> parseAttributes(
        const QDomElement& propertyList) const;

    nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadata> parse(const QDomElement& target);

    void evictStaleCacheEntries();

private:
    std::map<nx::sdk::Uuid, CacheEntry> m_cache;
};

} // namespace nx::vms_server_plugins::analytics::hikvision
