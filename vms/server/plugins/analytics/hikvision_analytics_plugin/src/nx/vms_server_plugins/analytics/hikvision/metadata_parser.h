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

class MetadataParser
{
public:
    void setHandler(nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> handler);

    void parsePacket(QByteArray bytes, std::int64_t timestampUs);
    void processEvent(HikvisionEvent* event, std::int64_t timestampUs);

private:
    struct MinMaxRect
    {
        nx::sdk::analytics::Point min;
        nx::sdk::analytics::Point max;
    };

    enum class State
    {
        initial,

        normalInitial,
        normalShouldGenerateBestShot,
        normalBestShotGenerated,

        thermalInitial,
        thermalShouldStartPreAlarm,
        thermalPreAlarmActive,
        thermalShouldStopPreAlarm,
        thermalShouldStartAlarm,
        thermalAlarmActive,
        thermalShouldStopAlarm,
    };

    struct CacheEntry
    {
        nx::sdk::Uuid trackId = nx::sdk::UuidHelper::randomUuid();
        QString typeId;

        State state = State::initial;

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
    std::int64_t m_timestampUs = -1;

    std::map<int, CacheEntry> m_cache;
};

} // namespace nx::vms_server_plugins::analytics::hikvision
