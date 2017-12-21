#include "naive_object_tracker.h"

#include <cmath>

#include <boost/optional/optional.hpp>

#define NX_PRINT_PREFIX "metadata::tegra_video::NaiveObjectTracker::"
#include <nx/kit/debug.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/random.h>
#include <nx/utils/uuid.h>

#include <plugins/plugin_tools.h>
#include <plugins/plugin_internal_tools.h>
#include <nx/sdk/metadata/common_detected_object.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/sdk/metadata/common_attribute.h>

#include "tegra_video_metadata_plugin_ini.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

namespace sdk = nx::sdk::metadata;

void NaiveObjectTracker::filterAndTrack(
    std::vector<nx::sdk::metadata::AbstractMetadataPacket*>* outMetadataPackets,
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
            addObjectToCache(QnUuid::createUuid(), rect);
    }

    auto packet = new sdk::CommonObjectsMetadataPacket();
    packet->setTimestampUsec(ptsUs);
    packet->setDurationUsec(1000000LL * 10);

    removeExpiredObjectsFromCache();
    addNonExpiredObjectsFromCache(packet);
    unmarkFoundObjectsInCache();

    outMetadataPackets->push_back(packet);
}

void NaiveObjectTracker::setObjectTypeId(const QnUuid& objectTypeId)
{
    m_objectTypeId = objectTypeId;
}

void NaiveObjectTracker::setAttributeOptions(
    const QString& attributeName,
    const std::vector<QString>& attributeValues)
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
        auto speed = itr->second.speed;
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

void NaiveObjectTracker::addObjectToCache(const QnUuid& id, const TegraVideo::Rect& boundingBox)
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

void NaiveObjectTracker::updateObjectInCache(const QnUuid& id, const TegraVideo::Rect& boundingBox)
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
    nx::sdk::metadata::CommonObjectsMetadataPacket* outPacket)
{
    for (auto& item: m_cachedObjects)
    {
        auto object = new nx::sdk::metadata::CommonDetectedObject();
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
                << cached.id.toString().toStdString() << " ("
                << cached.rect.x << " "
                << cached.rect.y << ") ("
                << bottomRightX(cached.rect) << " "
                << bottomRightY(cached.rect) << ")";
        }

        object->setBoundingBox(toSdkRect(cached.rect));
        object->setId(toSdkGuid(cached.id));
        object->setConfidence(1);
        object->setTypeId(toSdkGuid(m_objectTypeId));

        std::vector<sdk::CommonAttribute> attributes;
        for (const auto& entry: cached.attributes)
        {
            const auto attributeName = entry.first;
            const auto attributeValue = entry.second;

            sdk::CommonAttribute attribute(
                nx::sdk::AttributeType::string,
                attributeName.toStdString(),
                attributeValue.toStdString());

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

    auto center = rectangleCenter(rect);
    auto x = center.x();
    auto y = center.y();

    double xFirstZoneCorrection = (double)ini().postprocXfirstZoneCorrection / 1000;
    double yFirstZoneCorrection = (double)ini().postprocYfirstZoneCorrection / 1000;

    double xSecondZoneCorrection = (double)ini().postprocXsecondZoneCorrection / 1000;
    double ySecondZoneCorrection = (double)ini().postprocYsecondZoneCorrection / 1000;

    double xThirdZoneCorrection = (double)ini().postprocXthirdZoneCorrection / 1000;
    double yThirdZoneCorrection = (double)ini().postprocYthirdZoneCorrection / 1000;

    double firstZoneBound = (double)ini().postprocFirstZoneBound / 100;
    double secondZoneBound = (double)ini().postprocSecondZoneBound / 100;

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
    NX_ASSERT(outCachedObject);
    if (!outCachedObject)
        return;

    for (const auto& entry: m_attributeOptions)
    {
        const auto attributeName = entry.first;
        const auto attributeValue = randomAttributeValue(attributeName);

        if (attributeValue.isEmpty())
            return;

        outCachedObject->attributes[attributeName] = attributeValue;
    }
}

QString NaiveObjectTracker::randomAttributeValue(const QString& attributeName) const
{
    auto itr = m_attributeOptions.find(attributeName);
    if (itr == m_attributeOptions.cend() || itr->second.empty())
        return QString();

    const auto& options = itr->second;
    const auto index = nx::utils::random::number<int>(0, options.size() - 1);
    NX_ASSERT(index >= 0 && index < options.size());
    if (index < 0 && index >= options.size())
        return QString();

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

sdk::Rect NaiveObjectTracker::toSdkRect(const TegraVideo::Rect& rectangle)
{
    return sdk::Rect(rectangle.x, rectangle.y, rectangle.w, rectangle.h);
}

nxpl::NX_GUID NaiveObjectTracker::toSdkGuid(const QnUuid& id)
{
    return nxpt::fromQnUuidToPluginGuid(id);
}

} // namespace tegra_video
} // namespace mediaserver_plugins
} // namespace metadata
} // namespace nx
