#pragma once

#include "engine.h"

#include <chrono>
#include <unordered_map>
#include <string>
#include <optional>

#include <QString>
#include <QByteArray>
#include <QtXml/QDomDocument>

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

class ObjectMetadataXmlParser
{
public:
    struct Result
    {
        nx::sdk::Ptr<nx::sdk::analytics::EventMetadataPacket> eventMetadataPacket;
        nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> objectMetadataPacket;
    };

public:
    explicit ObjectMetadataXmlParser(
        const Hanwha::EngineManifest& engineManifest,
        const Hanwha::ObjectMetadataAttributeFilters& objectAttributeFilters);

    Result parse(const QByteArray& data);

private:
    using ObjectId = int;

    struct FrameScale
    {
        float x;
        float y;
    };

    class ObjectAttributes:
        public std::unordered_map<std::string, std::string>
    {
    public:
        std::chrono::steady_clock::time_point timeStamp;
    };

private:
    bool extractFrameScale(const QDomElement& transformation);

    [[nodiscard]] nx::sdk::analytics::Rect applyFrameScale(nx::sdk::analytics::Rect rect) const;

    std::optional<QString> filterAttribute(const QString& name) const;

    std::vector<nx::sdk::Ptr<nx::sdk::Attribute>> extractAttributes(
        ObjectId objectId, const QDomElement& appearance);

    nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> extractObjectMetadata(const QDomElement& object);
    nx::sdk::Ptr<nx::sdk::analytics::EventMetadata> extractEventMetadata(const QDomElement& object);

    void collectGarbage();

private:
    const Hanwha::EngineManifest& m_engineManifest;
    const Hanwha::ObjectMetadataAttributeFilters& m_objectAttributeFilters;
    FrameScale m_frameScale;
    std::unordered_map<ObjectId, ObjectAttributes> m_objectAttributes;
};

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
