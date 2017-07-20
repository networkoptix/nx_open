#pragma once

#include <QtGui/QVector2D>

#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace analytics {

class NaiveObjectTracker
{

public:
    void filterAndTrack(QnObjectDetectionMetadataPtr outMetadata);

private:
    struct CachedObject
    {
        QnUuid id;
        QnRect rect;
        int lifetime = kDefaultObjectLifetime;
        QVector2D speed; //< relative units per frame
        bool found = false;
    };

private:
    boost::optional<CachedObject> findAndMarkSameObjectInCache(const QnRect& boundingBox);
    void unmarkFoundObjectsInCache();
    void addObjectToCache(const QnUuid& id, const QnRect& boundingBox);
    void updateObjectInCache(const QnUuid& id, const QnRect& boundingBox);
    void removeExpiredObjectsFromCache();

    void addNonExpiredObjectsFromCache(std::vector<QnObjectDetectionInfo>& objects) const;
    QnRect applySpeedToRectangle(const QnRect& rectangle, const QVector2D& speed) const;
    QVector2D calculateSpeed(const QnRect& previousPosition, const QnRect& currentPosition) const;
    double distance(const QnRect& first, const QnRect& second) const;
    QPointF rectangleCenter(const QnRect& rect) const;
    QnRect correctRectangle(const QnRect& rect) const;
    double predictXSpeedForRectangle(const QnRect& rect) const;

private:
    std::map<QnUuid, CachedObject> m_cachedObjects;

};

} // namespace analytics
} // namespace nx
