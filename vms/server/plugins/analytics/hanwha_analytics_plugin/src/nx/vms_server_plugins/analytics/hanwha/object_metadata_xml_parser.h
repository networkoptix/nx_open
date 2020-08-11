#pragma once

#include "engine.h"

#include <chrono>
#include <unordered_map>
#include <string>
#include <optional>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtXml/QDomDocument>

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/utils/url.h>
#include <nx/sdk/helpers/uuid_helper.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

class ObjectMetadataXmlParser
{
public:
    struct Result
    {
        nx::sdk::Ptr<nx::sdk::analytics::EventMetadataPacket> eventMetadataPacket;
        nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> objectMetadataPacket;
        std::vector<nx::sdk::Ptr<nx::sdk::analytics::ObjectTrackBestShotPacket>> bestShotPackets;
    };

public:
    explicit ObjectMetadataXmlParser(
        nx::utils::Url baseUrl,
        const Hanwha::EngineManifest& engineManifest,
        const Hanwha::ObjectMetadataAttributeFilters& objectAttributeFilters);

    Result parse(const QByteArray& data, int64_t timestampUs);

private:

    struct FrameScale
    {
        float x;
        float y;
    };

    struct TrackData
    {
        std::unordered_map<std::string, std::string> attributes;
        std::chrono::steady_clock::time_point lastUpdateTime;
        std::optional<int64_t> trackStartTimeUs;
        nx::sdk::Uuid trackId;
    };

    using ObjectId = int;

    struct ObjectResult
    {
        nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> metadata;
        nx::sdk::Ptr<nx::sdk::analytics::ObjectTrackBestShotPacket> bestShotPacket;
        std::string objectTypeId;
        ObjectId objectId;
    };

private:
    bool extractFrameScale(const QDomElement& transformation);

    [[nodiscard]] nx::sdk::analytics::Rect applyFrameScale(nx::sdk::analytics::Rect rect) const;

    std::optional<QString> filterAttribute(const QString& name) const;

    std::vector<nx::sdk::Ptr<nx::sdk::Attribute>> extractAttributes(
        TrackData& trackData, const QDomElement& appearance);

    ObjectResult extractObjectMetadata(const QDomElement& object, std::int64_t timestampUs);

    void collectGarbage();

private:
    const nx::utils::Url m_baseUrl;
    const Hanwha::EngineManifest& m_engineManifest;
    const Hanwha::ObjectMetadataAttributeFilters& m_objectAttributeFilters;
    FrameScale m_frameScale;
    std::unordered_map<ObjectId, TrackData> m_objectTrackCache;
};

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
