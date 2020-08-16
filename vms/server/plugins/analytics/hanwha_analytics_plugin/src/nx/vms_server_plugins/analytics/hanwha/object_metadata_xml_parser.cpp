#include "object_metadata_xml_parser.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <optional>
#include <vector>

#include <nx/utils/log/log.h>
#include <nx/utils/match/wildcard.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/helpers/uuid_helper.h>

namespace nx::vms_server_plugins::analytics::hanwha {

using namespace std::string_literals;
using namespace std::chrono_literals;

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::utils;

namespace {
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------

struct ClassInfo
{
    std::string internalClassName; //< Expected one of "Person", "Vehicle", "Face", "LicensePlate".
    float confidence = 0.0; // Range [0.0 - 0.1].
};

//-------------------------------------------------------------------------------------------------


/**
 * Find first sub-element with tag `childElement`, where `childElement` doesn't contain  namespace.
 * Should be used instead of QDomElement::firstChildElement(), that needs child tag namespace,
 * Because some Hanwha devices emit xml metadata with errors in namespaces, so we ignore them.
 *
 * NB: In order to work with with function QDomDocument should be initialized with
 * `setContent(<xml_as_stirng>, true);` call. Pay attention at parameter `true`.
 */
QDomElement getChildElement(const QDomElement& parent, const QString& childTagName)
{
    for (QDomElement e = parent.firstChildElement(); !e.isNull(); e = e.nextSiblingElement())
    {
        if (e.tagName() == childTagName)
            return e;
    }
    return QDomElement();
}

//-------------------------------------------------------------------------------------------------

std::optional<Rect> extractRect(const QDomElement& shape)
{
    std::optional<Rect> result;
    QDomElement boundingBox = getChildElement(shape, "BoundingBox");
    if (boundingBox.isNull())
        return result;

    const float left = boundingBox.attribute("left").toFloat();
    const float top = boundingBox.attribute("top").toFloat();
    const float right = boundingBox.attribute("right").toFloat();
    const float bottom = boundingBox.attribute("bottom").toFloat();
    if (!left && !top && !right && !bottom)
        return result;

    result = { left, top, right - left, bottom - top };
    return result;
}

//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------

std::optional<ClassInfo> extractClassInfo(const QDomElement& class_)
{
    std::optional<ClassInfo> result;
    QDomElement type = getChildElement(class_, "Type");
    if (type.isNull())
        return result;

    const std::string internalName = type.text().toStdString();
    const float likelihood = type.attribute("Likelihood").toFloat() / 100;
    result = { internalName, likelihood};
    return result;
}

template <typename F>
void forEachChildElement(const QDomElement& element, const QString& tagName, F f)
{
    for (auto child = element.firstChildElement(tagName);
         !child.isNull(); child = child.nextSiblingElement(tagName))
    {
        f(child);
    }
}

//-------------------------------------------------------------------------------------------------

} // namespace

//-------------------------------------------------------------------------------------------------

ObjectMetadataXmlParser::ObjectMetadataXmlParser(
        Url baseUrl,
        const Hanwha::EngineManifest& engineManifest,
        const Hanwha::ObjectMetadataAttributeFilters& objectAttributeFilters):
    m_baseUrl(std::move(baseUrl)),
    m_engineManifest(engineManifest),
    m_objectAttributeFilters(objectAttributeFilters)
{
}

ObjectMetadataXmlParser::Result ObjectMetadataXmlParser::parse(
    const QByteArray& data, int64_t timestampUs)
{
    collectGarbage();

    QDomDocument dom;
    dom.setContent(data, true);

    const QDomElement root = dom.documentElement();
    if (root.tagName() != QStringLiteral("MetadataStream"))
        return {};

    const QDomElement videoAnalytics = getChildElement(root, "VideoAnalytics");
    if (videoAnalytics.isNull())
        return {}; // We ignore event metadata.

    const QDomElement frame = getChildElement(videoAnalytics, "Frame");
    if (frame.isNull())
        return {};

    const QDomElement transformation = getChildElement(frame, "Transformation");
    if (transformation.isNull())
        return {};

    if (!extractFrameScale(transformation))
        return {};

    auto eventPacket = makePtr<EventMetadataPacket>();
    auto objectPacket = makePtr<ObjectMetadataPacket>();
    std::vector<Ptr<ObjectTrackBestShotPacket>> bestShotPackets;

    for (auto object = frame.firstChildElement("Object");
         !object.isNull(); object = object.nextSiblingElement("Object"))
    {
        auto [objectMetadata, bestShotPacket, objectTypeId, objectId] =
            extractObjectMetadata(object, timestampUs);

        if (objectMetadata)
            objectPacket->addItem(objectMetadata.releasePtr());
        if (bestShotPacket)
        {
            auto eventData = makePtr<EventMetadata>();
            const auto& descriptor = m_engineManifest.objectTypeDescriptorById(QString::fromStdString(objectTypeId));
            auto eventTypeId = descriptor.id;
            eventTypeId.replace(QLatin1String("ObjectDetection."), QLatin1String("trackingEvent."));
            eventData->setTypeId(eventTypeId.toStdString());
            for (int i = 0; i < bestShotPacket->attributeCount(); ++i)
            {
                auto attribute = nx::sdk::makePtr<Attribute>(bestShotPacket->attribute(i));
                eventData->addAttribute(attribute);
            }
            eventData->setConfidence(1.0);
            eventData->setTrackId(bestShotPacket->trackId());
            eventPacket->addItem(eventData.releasePtr());

            auto& trackData = m_objectTrackCache[objectId];
            if (trackData.trackStartTimeUs)
                eventPacket->setTimestampUs(*trackData.trackStartTimeUs);

            bestShotPackets.push_back(std::move(bestShotPacket));
        }
    }

    if (eventPacket->count() == 0)
        eventPacket = nullptr;

    if (objectPacket->count() == 0)
    {
        objectPacket = nullptr;
    }
    else
    {
        objectPacket->setTimestampUs(timestampUs);
        objectPacket->setDurationUs(1'000'000);
    }

    return {
        std::move(eventPacket),
        std::move(objectPacket),
        std::move(bestShotPackets),
    };
}

bool ObjectMetadataXmlParser::extractFrameScale(const QDomElement& transformation)
{
    double xNumerator = 0.0;
    double yNumerator = 0.0;
    double xDenominator = 0.0;
    double yDenominator = 0.0;
    for (QDomElement e = transformation.firstChildElement(); !e.isNull(); e = e.nextSiblingElement())
    {
        if (e.tagName() == "Translate")
        {
            xDenominator = e.attribute("x").toFloat();
            yDenominator = e.attribute("y").toFloat();
        }
        else if (e.tagName() == "Scale")
        {
            xNumerator = e.attribute("x").toFloat();
            yNumerator = e.attribute("y").toFloat();
        }
    }
    if (xNumerator && yNumerator && xDenominator && yDenominator)
    {
        // The origin of this factor is unknown, but it works much better with it.
        static const float kScaleFactor = 2.0f;
        m_frameScale = {
            float(std::abs(xNumerator / xDenominator / kScaleFactor)),
            float(std::abs(yNumerator / yDenominator / kScaleFactor)),
        };
        return true;
    }
    return false;
}

[[nodiscard]] Rect ObjectMetadataXmlParser::applyFrameScale(Rect rect) const {
    return {
        rect.x * m_frameScale.x,
        rect.y * m_frameScale.y,
        rect.width * m_frameScale.x,
        rect.height * m_frameScale.y,
    };
}

std::optional<QString> ObjectMetadataXmlParser::filterAttribute(const QString& name) const
{
    for (const auto& pattern: m_objectAttributeFilters.discard)
    {
        if (wildcardMatch(pattern, name))
            return std::nullopt;
    }
    for (const auto& entry: m_objectAttributeFilters.rename)
    {
        if (wildcardMatch(entry.ifMatches, name))
            return entry.replaceWith;
    }
    return name;
}

std::vector<Ptr<Attribute>> ObjectMetadataXmlParser::extractAttributes(
    TrackData& trackData, const QDomElement& appearance)
{
    const auto walk =
        [&](auto walk, const QString& key, const QDomElement& element)
        {
            const auto domAttrs = element.attributes();
            for (int i = 0; i < domAttrs.length(); ++i)
            {
                const auto domAttr = domAttrs.item(i);
                const auto name = domAttr.nodeName();
                const auto value = domAttr.nodeValue();
                trackData.attributes[(key + ".@" + name).toStdString()] = value.toStdString();
            }

            std::unordered_map<std::string, int> counts;
            forEachChildElement(element, "",
                [&](const auto& child) {
                    ++counts[child.tagName().toStdString()];
                });

            if (counts.empty())
            {
                if (const auto child = element.firstChild(); child.isText())
                {
                    const QString value = child.toText().data();
                    trackData.attributes[key.toStdString()] = value.toStdString();
                }
            }

            std::unordered_map<std::string, int> indices;
            forEachChildElement(element, "",
                [&](const auto& child) {
                    const auto name  = child.tagName().toStdString();

                    QString suffix;
                    if (counts[name] > 1)
                        suffix = QString("[%1]").arg(++indices[name]);

                    walk(walk, key + "." + child.tagName() + suffix, child);
                });
        };

    for (const auto& rootTag: m_objectAttributeFilters.roots)
        forEachChildElement(appearance, rootTag,
            [&](const auto& element) {
                // Since it appears that single object cannot have multiple
                // root tags, we can just clear all the attributes.
                trackData.attributes.clear();

                walk(walk, rootTag, element);
            });


    std::vector<Ptr<Attribute>> result;
    result.reserve(trackData.attributes.size());
    for (const auto& [name, value]: trackData.attributes)
    {
        auto newName = filterAttribute(QString::fromStdString(name));
        if (newName)
        {
            auto attribute = makePtr<Attribute>(Attribute::Type::string, newName->toStdString(), value);
            result.push_back(std::move(attribute));
        }
    }
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b)
        {
            return std::strcmp(a->name(), b->name()) < 0;
        });

    return result;
}

ObjectMetadataXmlParser::ObjectResult ObjectMetadataXmlParser::extractObjectMetadata(
    const QDomElement& object, std::int64_t timestampUs)
{
    const ObjectId objectId = object.attribute("ObjectId").toInt();

    const QDomElement appearance = getChildElement(object, "Appearance");
    if (appearance.isNull())
        return {};

    const QDomElement shape = getChildElement(appearance, "Shape");
    if (shape.isNull())
        return {};

    const QDomElement class_ = getChildElement(appearance, "Class");
    if (class_.isNull())
        return {};

    const auto classInfo = extractClassInfo(class_);
    if (!classInfo)
        return {};

    const QString objectTypeName = QString::fromStdString(classInfo->internalClassName).trimmed();
    std::string objectTypeId = m_engineManifest.objectTypeIdByInternalName(objectTypeName).toStdString();
    if (objectTypeId.empty())
        return {};

    auto& trackData = m_objectTrackCache[objectId];
    trackData.lastUpdateTime = std::chrono::steady_clock::now();
    if (!trackData.trackStartTimeUs)
        trackData.trackStartTimeUs = timestampUs;
    if (trackData.trackId.isNull())
        trackData.trackId = nx::sdk::UuidHelper::randomUuid();

    const QDomElement imageRef = getChildElement(appearance, "ImageRef");

    Ptr<ObjectTrackBestShotPacket> bestShotPacket;
    Ptr<ObjectMetadata> metadata;

    const auto rect = extractRect(shape);
    Rect relativeRect;
    if (rect)
        relativeRect = applyFrameScale(*rect);

    const auto attributes = extractAttributes(trackData, appearance);

    if (!imageRef.isNull())
    {
        // For best shots it is allowed to have a null bounding box.
        Url url = imageRef.text();
        if (url.isRelative())
        {
            url = m_baseUrl;
            url.setPath(imageRef.text());
        }

        bestShotPacket = makePtr<ObjectTrackBestShotPacket>(trackData.trackId, timestampUs, relativeRect);
        bestShotPacket->setImageUrl(url.toStdString());
        bestShotPacket->addAttributes(attributes);
    }

    if (rect)
    {
        metadata = makePtr<ObjectMetadata>();
        metadata->setTrackId(trackData.trackId);
        metadata->setTypeId(objectTypeId);
        metadata->setBoundingBox(relativeRect);
        metadata->addAttributes(attributes);
    }


    return {std::move(metadata), std::move(bestShotPacket), objectTypeId, objectId};
}

void ObjectMetadataXmlParser::collectGarbage()
{
    static const auto timeout = 1min;

    const auto now = std::chrono::steady_clock::now();
    for (auto it = m_objectTrackCache.begin(); it != m_objectTrackCache.end(); )
    {
        auto [objectId, trackData] = *it;
        if (now - trackData.lastUpdateTime > timeout)
            it = m_objectTrackCache.erase(it);
        else
            ++it;
    }
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
