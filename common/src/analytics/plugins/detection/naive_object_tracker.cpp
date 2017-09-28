#include "naive_object_tracker.h"
#include "config.h"

#include <boost/optional/optional.hpp>

#define NX_OUTPUT_PREFIX "[nx::analytics::NaiveObjectTracker] "
#include <nx/kit/debug.h>

#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace analytics {

void NaiveObjectTracker::filterAndTrack(QnObjectDetectionMetadataPtr outMetadata)
{
    if (!ini().enableNaiveObjectTracking)
        return;

    std::vector<QnObjectDetectionInfo> filtered;
    for (auto& info: outMetadata->detectedObjects)
    {
        const auto& rect = info.boundingBox;
        auto width = rect.bottomRight.x - rect.topLeft.x;
        auto height = rect.bottomRight.y - rect.topLeft.y;

        bool rectangleIsTooSmall = (ini().minObjectWidth != 0
            && width < ini().minObjectWidth / 100)
            || ( ini().minObjectHeight != 0
            && height < ini().minObjectHeight / 100);

        if (rectangleIsTooSmall)
            continue;

        bool rectangleIsTooBig = (ini().maxObjectWidth != 0
            && width > ini().maxObjectWidth / 100)
            || (ini().maxObjectHeight != 0
            && height > ini().maxObjectHeight / 100);

        if (rectangleIsTooBig)
            continue;

        if (ini().enableNaiveObjectTracking)
        {
            if (auto sameObject = findAndMarkSameObjectInCache(info.boundingBox))
            {
                info.objectId = sameObject->id;
                updateObjectInCache(info.objectId, info.boundingBox);
            }
            else
            {
                info.objectId = QnUuid::createUuid();
                addObjectToCache(info.objectId, info.boundingBox);
            }
        }

        filtered.push_back(info);
    }

    if (ini().enableNaiveObjectTracking)
    {
        removeExpiredObjectsFromCache();
        addNonExpiredObjectsFromCache(filtered);
        unmarkFoundObjectsInCache();
    }

    std::swap(outMetadata->detectedObjects, filtered);
}

void NaiveObjectTracker::unmarkFoundObjectsInCache()
{
    for (auto& item: m_cachedObjects)
        item.second.found = false;
}

boost::optional<NaiveObjectTracker::CachedObject>
NaiveObjectTracker::findAndMarkSameObjectInCache(const QnRect& boundingBox)
{
    double minDistance = std::numeric_limits<double>::max();
    auto closestRect = m_cachedObjects.end();

    for (auto itr = m_cachedObjects.begin(); itr != m_cachedObjects.end(); ++itr)
    {
        if (itr->second.found)
            continue;

        auto& rect = itr->second.rect;
        auto speed = itr->second.speed;
        double currentDistance = std::numeric_limits<double>::max();
        if (ini().applySpeedForDistanceCalculation)
        {
            QVector2D speed = itr->second.speed;
            if (speed.isNull())
            {
                speed = QVector2D(
                    predictXSpeedForRectangle(itr->second.rect),
                    (double) ini().defaultYSpeed / 1000);
            }

            currentDistance = distance(
                applySpeedToRectangle(rect, speed), boundingBox);
        }
        else
        {
            currentDistance = distance(rect, boundingBox);
        }

        if (currentDistance < minDistance)
        {
            closestRect = itr;
            minDistance = currentDistance;
        }
    }

    auto minimalSimilarityDistance = (double) ini().similarityThreshold / 1000;
    bool sameObjectWasFound = closestRect != m_cachedObjects.end()
        && minDistance < minimalSimilarityDistance;

    if (sameObjectWasFound)
    {
        closestRect->second.found = true;
        return closestRect->second;
    }

    return boost::none;
}

void NaiveObjectTracker::addObjectToCache(const QnUuid& id, const QnRect& boundingBox)
{
    CachedObject object;
    object.id = id;
    object.rect = boundingBox;
    object.lifetime = ini().objectLifetime;
    object.found = true;

    m_cachedObjects[id] = object;
}

void NaiveObjectTracker::updateObjectInCache(const QnUuid& id, const QnRect& boundingBox)
{
    auto itr = m_cachedObjects.find(id);
    if (itr != m_cachedObjects.end())
    {
        auto& cached = itr->second;

        cached.speed = calculateSpeed(cached.rect, boundingBox);
        cached.rect = boundingBox;

        auto borderThreshold = (double)ini().bottomBorderBound / 100;
        if (cached.rect.bottomRight.y > borderThreshold ||
            cached.rect.bottomRight.x > borderThreshold ||
            cached.rect.topLeft.x < 1 - borderThreshold)
        {
            cached.lifetime = ini().bottomBorderLifetime;
        }
        else
        {
            cached.lifetime = ini().objectLifetime;
        }
    }
}

void NaiveObjectTracker::removeExpiredObjectsFromCache()
{
    for (auto itr = m_cachedObjects.begin(); itr != m_cachedObjects.end();)
    {
        auto& lifetime = itr->second.lifetime;
        if (lifetime == 0)
        {
            itr = m_cachedObjects.erase(itr);
        }
        else
        {
            --lifetime;
            ++itr;
        }
    }
}

void NaiveObjectTracker::addNonExpiredObjectsFromCache(
    std::vector<QnObjectDetectionInfo>& objects)
{
    for (auto& item: m_cachedObjects)
    {
        if (!item.second.found)
        {
            QnObjectDetectionInfo info;
            auto& cached = item.second;

            if (ini().applySpeedToCachedRectangles)
            {
                if (cached.speed.x() < cached.speed.y() || cached.speed.isNull())
                {
                    cached.rect = applySpeedToRectangle(cached.rect, QVector2D(
                        predictXSpeedForRectangle(cached.rect),
                        (double)ini().defaultYSpeed / 1000));

                    //TODO: #dmishin ask #mshevchenko about NX_OUTPUT
                    qDebug() << "(addNonExpiredObjects) "
                        << cached.id << " ("
                        << cached.rect.topLeft.x << " "
                        << cached.rect.topLeft.y << ") ("
                        << cached.rect.bottomRight.x << " "
                        << cached.rect.bottomRight.y << ")";
                }
            }

            info.boundingBox = cached.rect;
            info.objectId = cached.id;
            objects.push_back(info);
        }
    }
}

QnRect NaiveObjectTracker::applySpeedToRectangle(const QnRect& rectangle, const QVector2D& speed)
{
    QnRect result = rectangle;
    auto xOffset = speed.x();
    auto yOffset = speed.y();

    result.topLeft.x = std::max(std::min(result.topLeft.x + xOffset, 1.0), 0.0);
    result.bottomRight.x = std::max(std::min(result.bottomRight.x + xOffset, 1.0), 0.0);
    result.topLeft.y = std::max(std::min(result.topLeft.y + yOffset, 1.0), 0.0);
    result.bottomRight.y = std::max(std::min(result.bottomRight.y + yOffset, 1.0), 0.0);

    return result;
}

QVector2D NaiveObjectTracker::calculateSpeed(
    const QnRect& previousPosition,
    const QnRect& currentPosition)
{
    auto firstCenterX = (previousPosition.topLeft.x + previousPosition.bottomRight.x) / 2;
    auto firstCenterY = (previousPosition.topLeft.y + previousPosition.bottomRight.y) / 2;
    auto secondCenterX = (currentPosition.topLeft.x + currentPosition.bottomRight.x) / 2;
    auto secondCenterY = (currentPosition.topLeft.y + currentPosition.bottomRight.y) / 2;

    QVector2D speed;

    speed.setX(secondCenterX - firstCenterX);
    speed.setY(secondCenterY - firstCenterY);

    return speed;
}

double NaiveObjectTracker::distance(const QnRect& first, const QnRect& second)
{
    auto firstCenterX = (first.topLeft.x + first.bottomRight.x) / 2;
    auto firstCenterY = (first.topLeft.y + first.bottomRight.y) / 2;
    auto secondCenterX = (second.topLeft.x + second.bottomRight.x) / 2;
    auto secondCenterY = (second.topLeft.y + second.bottomRight.y) / 2;

    auto rectCenterDistance = std::sqrt(
        std::pow(firstCenterX - secondCenterX, 2) +
        std::pow(firstCenterY - secondCenterY, 2));

    auto centerVector = QVector2D(
        secondCenterX - firstCenterX,
        secondCenterY - secondCenterY);

    // Hack. We consider only objects that move forward to the viewer or have small
    // speed in opposite direction.
    auto dotProduct = QVector2D::dotProduct(
        QVector2D(0.0, 1.0),
        centerVector);

    if (dotProduct < 0 && rectCenterDistance > ini().maxBackVectorLength)
        return std::numeric_limits<double>::max();

    return rectCenterDistance - dotProduct;
}

QPointF NaiveObjectTracker::rectangleCenter(const QnRect& rect)
{
    QPointF center;
    center.setX((rect.topLeft.x + rect.bottomRight.x) / 2);
    center.setY((rect.topLeft.y + rect.bottomRight.y) / 2);

    return center;
}

QnRect NaiveObjectTracker::correctRectangle(const QnRect& rect)
{
    double xCorrection = 0;
    double yCorrection = 0;

    auto center = rectangleCenter(rect);
    auto x = center.x();
    auto y = center.y();

    double xFirstZoneCorrection = (double) ini().xFirstZoneCorrection / 1000;
    double yFirstZoneCorrection = (double) ini().yFirstZoneCorrection / 1000;

    double xSecondZoneCorrection = (double) ini().xSecondZoneCorrection / 1000;
    double ySecondZoneCorrection = (double) ini().ySecondZoneCorrection / 1000;

    double xThirdZoneCorrection = (double) ini().xThirdZoneCorrection / 1000;
    double yThirdZoneCorrection = (double) ini().yThirdZoneCorrection / 1000;

    double firstZoneBound = (double) ini().firstZoneBound / 100;
    double secondZoneBound = (double) ini().secondZoneBound / 100;

    if (x < firstZoneBound)
    {
        xCorrection = xFirstZoneCorrection;
        yCorrection = yFirstZoneCorrection;
    }
    else if (x < secondZoneBound)
    {
        xCorrection = xSecondZoneCorrection;
        yCorrection = ySecondZoneCorrection;
    }
    else
    {
        xCorrection = xThirdZoneCorrection;
        yCorrection = yThirdZoneCorrection;
    }

    QVector2D speed(xCorrection, yCorrection);

    return applySpeedToRectangle(rect, speed);
}

double NaiveObjectTracker::predictXSpeedForRectangle(const QnRect& rect)
{
    auto x = rectangleCenter(rect).x();

    if (x < (double) ini().firstZoneBound / 100)
        return (double) ini().xFirstZoneCorrection / 1000;
    else if (x > (double) ini().secondZoneBound / 100)
        return (double) ini().xThirdZoneCorrection / 1000;
    else
        return (double) ini().xSecondZoneCorrection / 1000;
}


} // namespace analytics
} // namespace nx
