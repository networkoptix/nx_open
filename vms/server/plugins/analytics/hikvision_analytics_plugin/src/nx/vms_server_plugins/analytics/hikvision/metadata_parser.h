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
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QXmlStreamReader>
#include <nx/utils/elapsed_timer.h>

namespace nx::vms_server_plugins::analytics::hikvision {

class MetadataParser
{
public:
    struct Result
    {
        nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> metadataPacket;
        std::vector<nx::sdk::Ptr<nx::sdk::analytics::ObjectTrackBestShotPacket>> bestShotPackets;
    };

public:
    Result parsePacket(QByteArray bytes);

private:
    struct MinMaxRect
    {
        nx::sdk::analytics::Point min;
        nx::sdk::analytics::Point max;
    };

    struct CacheEntry
    {
        bool bestShotGenerated = false;
        std::optional<nx::sdk::analytics::Rect> boundingBox;
        std::vector<nx::sdk::Ptr<nx::sdk::Attribute>> attributes;
        nx::sdk::Uuid trackId;
        nx::utils::ElapsedTimer lastUpdate;
        nx::utils::ElapsedTimer lifetime;

        CacheEntry()
        {
            lifetime.restart();
        }
    };

    struct TargetResult
    {
        nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> metadata;
        nx::sdk::Ptr<nx::sdk::analytics::ObjectTrackBestShotPacket> bestShotPacket;
    };

    struct TargetListResult
    {
        std::vector<nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>> metadatas;
        std::vector<nx::sdk::Ptr<nx::sdk::analytics::ObjectTrackBestShotPacket>> bestShotPackets;
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
    TargetResult parseTargetElement();
    TargetListResult parseTargetListElement();
    TargetListResult parseTargetDetectionElement();
    Result parseMetadataElement();

    void evictStaleCacheEntries();

private:
    QXmlStreamReader m_xml;
    std::map<int, CacheEntry> m_cache;
};

} // namespace nx::vms_server_plugins::analytics::hikvision
