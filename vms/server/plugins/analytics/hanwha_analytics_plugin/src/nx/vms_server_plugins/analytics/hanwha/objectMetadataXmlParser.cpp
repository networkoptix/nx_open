#include "objectMetadataXmlParser.h"

#include <cmath>
#include <ctime>
#include <optional>
#include <vector>

#include <nx/utils/log/log.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>

namespace nx::vms_server_plugins::analytics::hanwha {

using namespace std::chrono_literals;

using nx::sdk::Ptr;
using nx::sdk::makePtr;
using nx::sdk::Uuid;
using nx::sdk::Attribute;
using nx::sdk::analytics::ObjectMetadata;
using nx::sdk::analytics::IObjectMetadata;
using nx::sdk::analytics::ObjectMetadataPacket;
using nx::sdk::analytics::Rect;

namespace {
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------

struct ClassInfo
{
    std::string internalClassName; //< Expected one of "Person", "Vehicle", "Face", "LicensePlate".
    float confidence = 0.0; // Range [0.0 - 0.1].
};

//-------------------------------------------------------------------------------------------------

/** reinterpret some type T (actually int) as Uuid*/
template<class T>
Uuid deviceObjectNumberToUuid(T deviceObjectNumber)
{
    Uuid serverObjectNumber;
    memcpy(&serverObjectNumber, &deviceObjectNumber,
        std::min(sizeof(serverObjectNumber), sizeof(deviceObjectNumber)));
    return serverObjectNumber;
}

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

ObjectMetadataXmlParser::ObjectMetadataXmlParser(const Hanwha::EngineManifest& engineManifest):
    m_engineManifest(engineManifest)
{
}

Ptr<ObjectMetadataPacket> ObjectMetadataXmlParser::parse(const QByteArray& data)
{
    Ptr<ObjectMetadataPacket> result;

    QDomDocument dom;
    dom.setContent(data, true);

    const QDomElement root = dom.documentElement();
    if (root.tagName() != QStringLiteral("MetadataStream"))
        return result;

    const QDomElement videoAnalytics = getChildElement(root, "VideoAnalytics");
    if (videoAnalytics.isNull())
        return result; // We ignore event metadata.

    const QDomElement frame = getChildElement(videoAnalytics, "Frame");
    if (frame.isNull())
        return result;

    const QDomElement transformation = getChildElement(frame, "Transformation");
    if (transformation.isNull())
        return result;

    if (!extractFrameScale(transformation))
        return result;

    result = makePtr<ObjectMetadataPacket>();

    for (auto object = frame.firstChildElement("Object");
         !object.isNull(); object = object.nextSiblingElement("Object"))
    {
        if (Ptr<ObjectMetadata> objectMetadata = extractObject(object))
            result->addItem(objectMetadata.releasePtr());
    }

    return result;
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
        else
        if (e.tagName() == "Scale")
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

std::vector<Ptr<Attribute>> ObjectMetadataXmlParser::extractAttributes(
    ObjectId objectId, const QDomElement& appearance)
{
    auto& attributes = m_objectAttributes[objectId];

    const auto walk =
        [&](auto walk, QString key, const QDomElement& element)
        {
            static auto const appendDot =
                [](const QString& string) {
                    if (string.isEmpty())
                        return string;
                    return string + ".";
                };

            const auto domAttrs = element.attributes();
            for (int i = 0; i < domAttrs.length(); ++i) {
                const auto domAttr = domAttrs.item(i);
                const auto name = domAttr.nodeName();
                const auto value = domAttr.nodeValue();
                attributes[(appendDot(key) + "@" + name).toStdString()] = value.toStdString();
            }

            std::unordered_map<std::string, int> counts;
            forEachChildElement(element, "",
                [&](const auto& child) {
                    ++counts[child.tagName().toStdString()];
                });

            if (counts.empty())
            {
                if (const auto child = element.firstChild(); child.isText()) {
                    const QString value = child.toText().data();
                    attributes[key.toStdString()] = value.toStdString();
                }
            }

            std::unordered_map<std::string, int> indices;
            forEachChildElement(element, "",
                [&](const auto& child) {
                    const auto name  = child.tagName().toStdString();

                    QString suffix;
                    if (counts[name] > 1)
                        suffix = QString("[%1]").arg(++indices[name]);

                    walk(walk, appendDot(key) + child.tagName() + suffix, child);
                });
        };

    // This set might be incomplete.
    static const std::vector<QString> rootTags = {
        "HumanBody",
        "HumanFace",
        "VehicleInfo",
    };
    for (const auto& rootTag: rootTags)
        forEachChildElement(appearance, rootTag,
            [&](const auto& element) {
                // Since it appears that single object cannot have multiple
                // root tags, we can just clear all the attributes.
                attributes.clear();

                walk(walk, "", element);
            });

    attributes.timeStamp = std::chrono::steady_clock::now();

    std::vector<Ptr<Attribute>> result;
    result.reserve(attributes.size());
    for (const auto& [name, value]: attributes)
    {
        auto attribute = makePtr<Attribute>(Attribute::Type::string, name, value);
        result.push_back(std::move(attribute));
    }
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b)
        {
            return std::strcmp(a->name(), b->name()) < 0;
        });

    return result;
}

Ptr<ObjectMetadata> ObjectMetadataXmlParser::extractObject(const QDomElement& object)
{
    Ptr<ObjectMetadata> result;
    const int objectId = object.attribute("ObjectId").toInt();

    const QDomElement appearance = getChildElement(object, "Appearance");
    if (appearance.isNull())
        return result;

    const QDomElement shape = getChildElement(appearance, "Shape");
    if (shape.isNull())
        return result;

    const QDomElement class_ = getChildElement(appearance, "Class");
    if (class_.isNull())
        return result;

    const auto rect = extractRect(shape);
    const auto classInfo = extractClassInfo(class_);
    if (!rect || !classInfo)
        return result;

    std::string objectTypeId = m_engineManifest.objectTypeIdByInternalName(
        QString::fromStdString(classInfo->internalClassName)).toStdString();
    if (objectTypeId.empty())
        return result;

    result = makePtr<ObjectMetadata>();

    const nx::sdk::Uuid trackId = deviceObjectNumberToUuid(objectId);
    result->setTrackId(trackId);

    result->setTypeId(objectTypeId);

    const Rect relativeRect = applyFrameScale(*rect);
    result->setBoundingBox(relativeRect);

    result->addAttributes(extractAttributes(objectId, appearance));

    return result;
}

void ObjectMetadataXmlParser::collectGarbage()
{
    static const auto timeout = 5min;

    auto now = std::chrono::steady_clock::now();
    for (auto it = m_objectAttributes.begin(); it != m_objectAttributes.end(); ) {
        auto [objectId, attributes] = *it;
        if (now - attributes.timeStamp > timeout)
            it = m_objectAttributes.erase(it);
        else
            ++it;
    }
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
