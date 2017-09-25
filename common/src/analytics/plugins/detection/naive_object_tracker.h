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
        int lifetime = 1;
        QVector2D speed; //< relative units per frame
        bool found = false;
    };

private:
    boost::optional<CachedObject> findAndMarkSameObjectInCache(const QnRect& boundingBox);
    void unmarkFoundObjectsInCache();
    void addObjectToCache(const QnUuid& id, const QnRect& boundingBox);
    void updateObjectInCache(const QnUuid& id, const QnRect& boundingBox);
    void removeExpiredObjectsFromCache();

    void addNonExpiredObjectsFromCache(std::vector<QnObjectDetectionInfo>& objects);
    QnRect applySpeedToRectangle(const QnRect& rectangle, const QVector2D& speed);
    QVector2D calculateSpeed(const QnRect& previousPosition, const QnRect& currentPosition);
    double distance(const QnRect& first, const QnRect& second);
    QPointF rectangleCenter(const QnRect& rect);
    QnRect correctRectangle(const QnRect& rect);
    double predictXSpeedForRectangle(const QnRect& rect);

private:
    std::map<QnUuid, CachedObject> m_cachedObjects;

};

} // namespace analytics
} // namespace nx
