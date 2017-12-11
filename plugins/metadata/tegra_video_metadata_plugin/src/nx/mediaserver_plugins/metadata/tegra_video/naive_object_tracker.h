#pragma once

#include <QtGui/QVector2D>

#include <tegra_video.h> //< libtegra_video.so - analytics lib for Tx1 and Tx2.
#include <boost/uuid/uuid.hpp>

#include <nx/utils/uuid.h>

#include <nx/sdk/metadata/abstract_metadata_packet.h>
#include <nx/sdk/metadata/common_metadata_packet.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

class NaiveObjectTracker
{

public:
    void filterAndTrack(
        std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* outMetadataPackets,
        const std::vector<TegraVideo::Rect>& rects,
        int64_t ptsUs);

    void setObjectTypeId(const QnUuid& objectTypeId);

    void setAttributeOptions(
        const QString& attributeName,
        const std::vector<QString>& attributeValues);

private:
    struct CachedObject
    {
        QnUuid id;
        TegraVideo::Rect rect;
        int lifetime = 1;
        QVector2D speed; //< relative units per frame
        bool found = false;
        std::map<QString, QString> attributes;
    };

private:
    boost::optional<CachedObject> findAndMarkSameObjectInCache(
        const TegraVideo::Rect& boundingBox);

    void unmarkFoundObjectsInCache();

    void addObjectToCache(const QnUuid& id, const TegraVideo::Rect& boundingBox);

    void updateObjectInCache(const QnUuid& id, const TegraVideo::Rect& boundingBox);

    void removeExpiredObjectsFromCache();

    void addNonExpiredObjectsFromCache(
        nx::sdk::metadata::CommonObjectsMetadataPacket* outPacket);

    TegraVideo::Rect applySpeedToRectangle(
        const TegraVideo::Rect& rectangle,
        const QVector2D& speed);

    QVector2D calculateSpeed(
        const TegraVideo::Rect& previousPosition,
        const TegraVideo::Rect& currentPosition);

    double distance(const TegraVideo::Rect& first, const TegraVideo::Rect& second);

    QPointF rectangleCenter(const TegraVideo::Rect& rect);

    TegraVideo::Rect correctRectangle(const TegraVideo::Rect& rect);

    double predictXSpeedForRectangle(const TegraVideo::Rect& rect);

    void assignRandomAttributes(CachedObject* outCachedObject);

    QString randomAttributeValue(const QString& attributeName) const;

    static bool isTooBig(const TegraVideo::Rect& rectangle);
    static bool isTooSmall(const TegraVideo::Rect& rectangle);
    static float bottomRightX(const TegraVideo::Rect& rectangle);
    static float bottomRightY(const TegraVideo::Rect& rectangle);
    static nx::sdk::metadata::Rect toSdkRect(const TegraVideo::Rect& rectangle);
    static nxpl::NX_GUID toSdkGuid(const QnUuid& id);

private:
    std::map<QnUuid, CachedObject> m_cachedObjects;
    QnUuid m_objectTypeId;
    std::map<QString, std::vector<QString>> m_attributeOptions;
};

} // namespace tegra_video
} // namespace mediaserver_plugins
} // namespace metadata
} // namespace nx
