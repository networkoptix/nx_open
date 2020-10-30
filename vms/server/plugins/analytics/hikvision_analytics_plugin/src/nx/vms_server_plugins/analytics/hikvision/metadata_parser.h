#pragma once

#include <cstdint>
#include <chrono>
#include <optional>
#include <map>
#include <vector>

#include <nx/utils/elapsed_timer.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/analytics/point.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QXmlStreamReader>

#include "common.h"

namespace nx::vms_server_plugins::analytics::hikvision {

extern const QString kThermalObjectTypeId;
extern const QString kThermalObjectPreAlarmTypeId;
extern const QString kThermalObjectAlarmTypeId;

class MetadataParser
{
public:
    void setHandler(nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> handler);

    void parsePacket(QByteArray bytes, std::int64_t timestampUs);
    void processEvent(HikvisionEvent* event);

private:
    struct MinMaxRect
    {
        nx::sdk::analytics::Point min;
        nx::sdk::analytics::Point max;
    };

    struct CacheEntry
    {
        nx::sdk::Uuid trackId;
        QString typeId;

        bool regularBestShotShouldBeGenerated = false;
        bool regularBestShotIsGenerated = false;

        bool thermalPreAlarmShouldBeActive = false;
        bool thermalPreAlarmIsActive = false;

        bool thermalAlarmShouldBeActive = false;
        bool thermalAlarmIsActive = false;

        std::optional<nx::sdk::analytics::Rect> boundingBox;
        std::vector<nx::sdk::Ptr<nx::sdk::Attribute>> attributes;

        nx::utils::ElapsedTimer lastUpdate;
        nx::utils::ElapsedTimer lifetime;
    };

private:
    std::optional<QString> parseStringElement();
    std::optional<int> parseIntElement();
    std::optional<int> parseTargetIdElement();
    std::optional<QString> parseRecognitionElement();
    std::optional<float> parseCoordinateElement();
    std::optional<nx::sdk::analytics::Point> parsePointElement();
    std::optional<MinMaxRect> parseRegionElement();
    std::optional<nx::sdk::analytics::Rect> parseRegionListElement();
    nx::sdk::Ptr<nx::sdk::Attribute> parsePropertyElement();
    std::vector<nx::sdk::Ptr<nx::sdk::Attribute>> parsePropertyListElement();
    void parseTargetElement();
    void parseTargetListElement();
    void parseTargetDetectionElement();
    void parseMetadataElement();

    nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> makeMetadataPacket(
        const CacheEntry& entry);

    void processEntry(CacheEntry* entry, HikvisionEvent* event = nullptr);

    void evictStaleCacheEntries();

private:
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;

    QXmlStreamReader m_xml;
    std::int64_t m_lastMetadataTimestampUs = -1;
    std::int64_t m_currentTimestampUs = -1;
    nx::utils::ElapsedTimer m_sinceLastMetadata;

    std::map<int, CacheEntry> m_cache;
};

} // namespace nx::vms_server_plugins::analytics::hikvision
