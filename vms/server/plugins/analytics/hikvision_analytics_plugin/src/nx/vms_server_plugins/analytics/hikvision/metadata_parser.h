#pragma once

#include <cstdint>
#include <chrono>
#include <optional>
#include <map>
#include <vector>

#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/analytics/point.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QXmlStreamReader>
#include <nx/utils/elapsed_timer.h>

namespace nx::vms_server_plugins::analytics::hikvision {

class MetadataParser
{
public:
    nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> parsePacket(QByteArray bytes);

private:
    struct MinMaxRect
    {
        nx::sdk::analytics::Point min;
        nx::sdk::analytics::Point max;
    };

    struct CacheEntry
    {
        CacheEntry()
        {
            lifetime.restart();
        }

        std::optional<nx::sdk::analytics::Rect> boundingBox;
        std::vector<nx::sdk::Ptr<nx::sdk::Attribute>> attributes;
        nx::sdk::Uuid trackId;
        nx::utils::ElapsedTimer lastUpdate;
        nx::utils::ElapsedTimer lifetime;
    };

private:
    std::optional<QString> parseStringElement();
    std::optional<int> parseIntElement();
    std::optional<int> parseTargetIdElement();
    std::optional<std::string> parseRecognitionElement();
    std::optional<float> parseCoordinateElement();
    std::optional<nx::sdk::analytics::Point> parsePointElement();
    std::optional<MinMaxRect> parseRegionElement();
    std::optional<nx::sdk::analytics::Rect> parseRegionListElement();
    nx::sdk::Ptr<nx::sdk::Attribute> parsePropertyElement();
    std::vector<nx::sdk::Ptr<nx::sdk::Attribute>> parsePropertyListElement();
    nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> parseTargetElement();
    std::vector<nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>> parseTargetListElement();
    std::vector<nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>> parseTargetDetectionElement();
    nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> parseMetadataElement();

    void evictStaleCacheEntries();

private:
    QXmlStreamReader m_xml;
    std::map<int, CacheEntry> m_cache;
};

} // namespace nx::vms_server_plugins::analytics::hikvision
