#include "naive_object_tracker.h"

#include <cmath>
#include <cstdlib>

#include <boost/optional/optional.hpp>

#define NX_PRINT_PREFIX "metadata::tegra_video::NaiveObjectTracker::"
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/analytics/helpers/object.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/attribute.h>

#include "tegra_video_analytics_plugin_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

void NaiveObjectTracker::filterAndTrack(
    std::vector<nx::sdk::analytics::IMetadataPacket*>* outMetadataPackets,
    const std::vector<TegraVideo::Rect>& rects,
    int64_t ptsUs)
{
    for (const auto& rect: rects)
    {
        if (isTooBig(rect) || isTooSmall(rect))
            continue;

        if (auto sameObject = findAndMarkSameObjectInCache(rect))
            updateObjectInCache(sameObject->id, rect);
        else
            addObjectToCache(nx::sdk::UuidHelper::randomUuid(), rect);
    }

    auto packet = new nx::sdk::analytics::ObjectMetadataPacket();
    packet->setTimestampUs(ptsUs);
    packet->setDurationUs(1000000LL * 10);

    removeExpiredObjectsFromCache();
    addNonExpiredObjectsFromCache(packet);
    unmarkFoundObjectsInCache();

    outMetadataPackets->push_back(packet);
}

void NaiveObjectTracker::setObjectTypeId(const std::string& objectTypeId)
{
    m_objectTypeId = objectTypeId;
}

void NaiveObjectTracker::setAttributeOptions(
    const std::string& attributeName,
    const std::vector<std::string>& attributeValues)
{
    m_attributeOptions[attributeName] = attributeValues;
}

void NaiveObjectTracker::unmarkFoundObjectsInCache()
{
    for (auto& item : m_cachedObjects)
        item.second.found = false;
}

boost::optional<NaiveObjectTracker::CachedObject>
NaiveObjectTracker::findAndMarkSameObjectInCache(const TegraVideo::Rect& boundingBox)
{
    double minDistance = std::numeric_limits<double>::max();
    auto closestRect = m_cachedObjects.end();

    for (auto itr = m_cachedObjects.begin(); itr != m_cachedObjects.end(); ++itr)
    {
        if (itr->second.found)
            continue;

        auto& rect = itr->second.rect;
        //auto speed = itr->second.speed;
        double currentDistance = std::numeric_limits<double>::max();
        if (ini().postprocApplySpeedForDistanceCalculation)
        {
            QVector2D speed = itr->second.speed;
            if (speed.isNull())
            {
                speed = QVector2D(
                    predictXSpeedForRectangle(itr->second.rect),
                    (double) ini().postprocDefaultYSpeed / 1000);
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

    auto minimalSimilarityDistance = (double)ini().postprocSimilarityThreshold / 1000;
    bool sameObjectWasFound = closestRect != m_cachedObjects.end()
        && minDistance < minimalSimilarityDistance;

    if (sameObjectWasFound)
    {
        closestRect->second.found = true;
        return closestRect->second;
    }

    return boost::none;
}

void NaiveObjectTracker::addObjectToCache(
    const nx::sdk::Uuid& id, const TegraVideo::Rect& boundingBox)
{
    CachedObject object;
    object.id = id;
    object.rect = boundingBox;
    object.lifetime = isRectangleInNoCorrectionZone(boundingBox)
        ? 1
        : ini().postprocObjectLifetime;

    object.found = true;

    assignRandomAttributes(&object);

    m_cachedObjects[id] = object;
}

void NaiveObjectTracker::updateObjectInCache(
    const nx::sdk::Uuid& id, const TegraVideo::Rect& boundingBox)
{
    auto itr = m_cachedObjects.find(id);
    if (itr == m_cachedObjects.cend())
        return;

    auto& cached = itr->second;
    cached.rect = boundingBox;
    cached.speed = calculateSpeed(cached.rect, boundingBox);

    const auto borderThreshold = (double)ini().postprocBottomBorderBound / 100;
    const bool isNearBorder = bottomRightX(cached.rect) > borderThreshold
        || bottomRightY(cached.rect) > borderThreshold
        || cached.rect.x < 1 - borderThreshold;

    if (isNearBorder)
        cached.lifetime = ini().postprocBottomBorderLifetime;
    else if (isRectangleInNoCorrectionZone(boundingBox))
        cached.lifetime = 1;
    else
        cached.lifetime = ini().postprocObjectLifetime;
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
    nx::sdk::analytics::ObjectMetadataPacket* outPacket)
{
    for (auto& item: m_cachedObjects)
    {
        auto object = new nx::sdk::analytics::Object();
        auto& cached = item.second;

        bool needToApplySpeed = ini().postprocApplySpeedToCachedRectangles
            && (cached.speed.x() < cached.speed.y() || cached.speed.isNull())
            && !cached.found;

        if (needToApplySpeed)
        {
            const auto speed = QVector2D(
                predictXSpeedForRectangle(cached.rect),
                (double)ini().postprocDefaultYSpeed / 1000);

            cached.rect = applySpeedToRectangle(cached.rect, speed);

            NX_OUTPUT << "(addNonExpiredObjects) "
                << nx::sdk::UuidHelper::toStdString(cached.id) << " ("
                << cached.rect.x << " "
                << cached.rect.y << ") ("
                << bottomRightX(cached.rect) << " "
                << bottomRightY(cached.rect) << ")";
        }

        object->setBoundingBox(toSdkRect(cached.rect));
        object->setId(cached.id);
        object->setConfidence(1);
        object->setTypeId(m_objectTypeId);

        std::vector<nx::sdk::analytics::Attribute> attributes;
        for (const auto& entry: cached.attributes)
        {
            const auto attributeName = entry.first;
            const auto attributeValue = entry.second;

            nx::sdk::analytics::Attribute attribute(
                nx::sdk::IAttribute::Type::string,
                attributeName,
                attributeValue);

            attributes.push_back(attribute);
        }

        object->setAttributes(attributes);
        outPacket->addItem(object);
    }
}

TegraVideo::Rect NaiveObjectTracker::applySpeedToRectangle(
    const TegraVideo::Rect& rectangle,
    const QVector2D& speed)
{
    TegraVideo::Rect result = rectangle;
    auto xOffset = speed.x();
    auto yOffset = speed.y();

    result.x = std::max(std::min(result.x + xOffset, 1.0f), 0.0f);
    result.y = std::max(std::min(result.y + yOffset, 1.0f), 0.0f);

    result.w = std::min(result.w, 1.0f - result.x);
    result.h = std::min(result.h, 1.0f - result.y);

    return result;
}

QVector2D NaiveObjectTracker::calculateSpeed(
    const TegraVideo::Rect& previousPosition,
    const TegraVideo::Rect& currentPosition)
{
    auto firstCenterX = (previousPosition.x + bottomRightX(previousPosition)) / 2;
    auto firstCenterY = (previousPosition.y + bottomRightY(previousPosition)) / 2;
    auto secondCenterX = (currentPosition.x + bottomRightX(currentPosition)) / 2;
    auto secondCenterY = (currentPosition.y + bottomRightY(currentPosition)) / 2;

    QVector2D speed;
    speed.setX(secondCenterX - firstCenterX);
    speed.setY(secondCenterY - firstCenterY);

    return speed;
}

double NaiveObjectTracker::distance(const TegraVideo::Rect& first, const TegraVideo::Rect& second)
{
    auto firstCenterX = (first.x + bottomRightX(second)) / 2;
    auto firstCenterY = (first.y + bottomRightY(second)) / 2;
    auto secondCenterX = (second.x + bottomRightX(second)) / 2;
    auto secondCenterY = (second.y + bottomRightY(second)) / 2;

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

    if (dotProduct < 0 && rectCenterDistance > ini().postprocMaxBackVectorLength)
        return std::numeric_limits<double>::max();

    return rectCenterDistance - dotProduct;
}

QPointF NaiveObjectTracker::rectangleCenter(const TegraVideo::Rect& rect) const
{
    QPointF center;
    center.setX((rect.x + bottomRightX(rect)) / 2);
    center.setY((rect.y + bottomRightY(rect)) / 2);

    return center;
}

TegraVideo::Rect NaiveObjectTracker::correctRectangle(const TegraVideo::Rect& rect)
{
    double xCorrection = 0;
    double yCorrection = 0;

    const auto center = rectangleCenter(rect);
    const auto x = center.x();
    //const auto y = center.y();

    const double xFirstZoneCorrection = ini().postprocXfirstZoneCorrection / 1000.0;
    const double yFirstZoneCorrection = ini().postprocYfirstZoneCorrection / 1000.0;

    const double xSecondZoneCorrection = ini().postprocXsecondZoneCorrection / 1000.0;
    const double ySecondZoneCorrection = ini().postprocYsecondZoneCorrection / 1000.0;

    const double xThirdZoneCorrection = ini().postprocXthirdZoneCorrection / 1000.0;
    const double yThirdZoneCorrection = ini().postprocYthirdZoneCorrection / 1000.0;

    const double firstZoneBound = ini().postprocFirstZoneBound / 100.0;
    const double secondZoneBound = ini().postprocSecondZoneBound / 100.0;

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

double NaiveObjectTracker::predictXSpeedForRectangle(const TegraVideo::Rect& rect)
{
    auto x = rectangleCenter(rect).x();

    if (x < (double)ini().postprocFirstZoneBound / 100)
        return (double)ini().postprocXfirstZoneCorrection / 1000;
    else if (x > (double) ini().postprocSecondZoneBound / 100)
        return (double)ini().postprocXthirdZoneCorrection / 1000;
    else
        return (double)ini().postprocXsecondZoneCorrection / 1000;
}

void NaiveObjectTracker::assignRandomAttributes(CachedObject* outCachedObject)
{
    NX_KIT_ASSERT(outCachedObject);
    if (!outCachedObject)
        return;

    for (const auto& entry: m_attributeOptions)
    {
        const auto attributeName = entry.first;
        const auto attributeValue = randomAttributeValue(attributeName);

        if (attributeValue.empty())
            return;

        outCachedObject->attributes[attributeName] = attributeValue;
    }
}

std::string NaiveObjectTracker::randomAttributeValue(const std::string& attributeName) const
{
    auto itr = m_attributeOptions.find(attributeName);
    if (itr == m_attributeOptions.cend() || itr->second.empty())
        return std::string();

    const auto& options = itr->second;
    const int index = std::rand() % options.size();
    NX_KIT_ASSERT(index >= 0 && index < (int) options.size());
    if (index < 0 && index >= (int) options.size())
        return std::string();

    return options[index];
}

bool NaiveObjectTracker::isRectangleInNoCorrectionZone(const TegraVideo::Rect& rect) const
{
    auto center = rectangleCenter(rect);

    const auto alpha = (double) ini().postProcNoCorrectionAlpha / 100;
    const auto beta = (double) ini().postProcNoCorrectionBeta / 100;

    const auto x = center.x();
    const auto y  = center.y();

    return y < x * alpha + beta;
}

bool NaiveObjectTracker::isTooBig(const TegraVideo::Rect& rectangle)
{
    return
        (ini().postprocMaxObjectWidth != 0
            && rectangle.w > (float) ini().postprocMaxObjectWidth / 100
        ) ||
        (ini().postprocMaxObjectHeight != 0
            && rectangle.h > (float) ini().postprocMaxObjectHeight / 100
        );
}

bool NaiveObjectTracker::isTooSmall(const TegraVideo::Rect& rectangle)
{
    return
        (ini().postprocMinObjectWidth != 0
            && rectangle.w < (float) ini().postprocMinObjectWidth / 100
        ) ||
        (ini().postprocMinObjectHeight != 0
            && rectangle.h < (float) ini().postprocMinObjectHeight / 100
        );
}

float NaiveObjectTracker::bottomRightX(const TegraVideo::Rect& rectangle)
{
    return rectangle.x + rectangle.w;
}

float NaiveObjectTracker::bottomRightY(const TegraVideo::Rect& rectangle)
{
    return rectangle.y + rectangle.h;
}

nx::sdk::analytics::IObject::Rect NaiveObjectTracker::toSdkRect(const TegraVideo::Rect& rectangle)
{
    return nx::sdk::analytics::IObject::Rect(rectangle.x, rectangle.y, rectangle.w, rectangle.h);
}

} // namespace tegra_video
} // namespace vms_server_plugins
} // namespace analytics
} // namespace nx
