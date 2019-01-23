#pragma once

#include <string>

#include <QtGui/QVector2D>

#include <boost/optional.hpp>

#include <tegra_video.h> //< libtegra_video.so - analytics lib for Tx1 and Tx2.

#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

class NaiveObjectTracker
{

public:
    void filterAndTrack(
        std::vector<nx::sdk::analytics::IMetadataPacket*>* outMetadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs);

    void setObjectTypeId(const std::string& objectTypeId);

    void setAttributeOptions(
        const std::string& attributeName,
        const std::vector<std::string>& attributeValues);

private:
    struct CachedObject
    {
        nx::sdk::Uuid id;
        TegraVideo::Rect rect;
        int lifetime = 1;
        QVector2D speed; //< relative units per frame
        bool found = false;
        std::map<std::string, std::string> attributes;
    };

private:
    boost::optional<CachedObject> findAndMarkSameObjectInCache(
        const TegraVideo::Rect& boundingBox);

    void unmarkFoundObjectsInCache();

    void addObjectToCache(const nx::sdk::Uuid& id, const TegraVideo::Rect& boundingBox);

    void updateObjectInCache(const nx::sdk::Uuid& id, const TegraVideo::Rect& boundingBox);

    void removeExpiredObjectsFromCache();

    void addNonExpiredObjectsFromCache(
        nx::sdk::analytics::ObjectMetadataPacket* outPacket);

    TegraVideo::Rect applySpeedToRectangle(
        const TegraVideo::Rect& rectangle,
        const QVector2D& speed);

    QVector2D calculateSpeed(
        const TegraVideo::Rect& previousPosition,
        const TegraVideo::Rect& currentPosition);

    double distance(const TegraVideo::Rect& first, const TegraVideo::Rect& second);

    QPointF rectangleCenter(const TegraVideo::Rect& rect) const;

    TegraVideo::Rect correctRectangle(const TegraVideo::Rect& rect);

    double predictXSpeedForRectangle(const TegraVideo::Rect& rect);

    void assignRandomAttributes(CachedObject* outCachedObject);

    std::string randomAttributeValue(const std::string& attributeName) const;

    bool isRectangleInNoCorrectionZone(const TegraVideo::Rect& rect) const;

    static bool isTooBig(const TegraVideo::Rect& rectangle);
    static bool isTooSmall(const TegraVideo::Rect& rectangle);
    static float bottomRightX(const TegraVideo::Rect& rectangle);
    static float bottomRightY(const TegraVideo::Rect& rectangle);
    static nx::sdk::analytics::IObject::Rect toSdkRect(const TegraVideo::Rect& rectangle);

private:
    std::map<nx::sdk::Uuid, CachedObject> m_cachedObjects;
    std::string m_objectTypeId;
    std::map<std::string, std::vector<std::string>> m_attributeOptions;
};

} // namespace tegra_video
} // namespace vms_server_plugins
} // namespace analytics
} // namespace nx
