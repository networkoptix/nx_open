#pragma once

#include "engine.h"

#include <chrono>
#include <unordered_map>
#include <string>

#include <QByteArray>
#include <QtXml/QDomDocument>

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

class ObjectMetadataXmlParser
{
public:
    explicit ObjectMetadataXmlParser(const Hanwha::EngineManifest& engineManifest);

    nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> parse(const QByteArray& data);

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
        std::chrono::steady_clock::time_point timeStamp = {};
    };

private:
    bool extractFrameScale(const QDomElement& transformation);

    [[nodiscard]] nx::sdk::analytics::Rect applyFrameScale(nx::sdk::analytics::Rect rect) const;

    std::vector<nx::sdk::Ptr<nx::sdk::Attribute>> extractAttributes(
        ObjectId objectId, const QDomElement& appearance);

    nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> extractObject(const QDomElement& object);

    void collectGarbage();

private:
    const Hanwha::EngineManifest& m_engineManifest;
    FrameScale m_frameScale;
    std::unordered_map<ObjectId, ObjectAttributes> m_objectAttributes;
};

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
