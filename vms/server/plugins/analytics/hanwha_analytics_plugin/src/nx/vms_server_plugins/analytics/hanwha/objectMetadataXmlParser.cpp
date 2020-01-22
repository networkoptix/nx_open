#include "objectMetadataXmlParser.h"

#include <charconv>
#include <cmath>

#include <QtXml/QDomDocument>

#include <nx/utils/log/log.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>

namespace nx::vms_server_plugins::analytics::hanwha {

using nx::sdk::Ptr;
using nx::sdk::makePtr;
using nx::sdk::Uuid;
using nx::sdk::Attribute;
using nx::sdk::analytics::ObjectMetadata;
using nx::sdk::analytics::IObjectMetadata;
using nx::sdk::analytics::ObjectMetadataPacket;
using nx::sdk::analytics::Rect;
//-------------------------------------------------------------------------------------------------

struct FrameScales
{
    float x;
    float y;
};

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

std::chrono::time_point<std::chrono::system_clock> QDateTimeToTimePoint(QDateTime dateTime)
{
    const qint64 millisec = dateTime.toMSecsSinceEpoch();
    const std::chrono::time_point<std::chrono::system_clock> epocBegin{};
    return epocBegin + std::chrono::milliseconds(millisec);
}

//-------------------------------------------------------------------------------------------------
std::string timePointToString(std::chrono::time_point<std::chrono::system_clock> timePoint)
{
    std::time_t epoch_time = std::chrono::system_clock::to_time_t(timePoint);
    return std::ctime(&epoch_time);
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

std::optional<FrameScales> extractScales(const QDomElement& transformation)
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
    std::optional<FrameScales> result;
    if (xNumerator && yNumerator && xDenominator && yDenominator)
    {
        // The origin of this factor is unknown, but it works much better with it.
        static const float kScaleFactor= 2.0f;
        result = { float(std::abs(xNumerator / xDenominator / kScaleFactor)),
            float(std::abs(yNumerator / yDenominator / kScaleFactor)) };
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

std::optional<QDateTime> extractTimestamp(const QDomElement& frame)
{
    QString deviceTimestampAsString = frame.attribute("UtcTime");
    QDateTime deviceTimestamp = QDateTime::fromString(deviceTimestampAsString, Qt::ISODateWithMs);
    std::optional<QDateTime> result;
    if (deviceTimestamp != QDateTime())
        result = deviceTimestamp;
    return result;
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

[[nodiscard]] Rect scale(Rect rect, FrameScales frameScales)
{

    return { rect.x * frameScales.x, rect.y * frameScales.y,
        rect.width * frameScales.x, rect.height * frameScales.y };
}

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
//-------------------------------------------------------------------------------------------------
Ptr<ObjectMetadata> extractObject(
    const QDomElement& object, FrameScales frameScales, const Hanwha::EngineManifest& manifest)
{
    Ptr<ObjectMetadata> result;
    int objectId = object.attribute("ObjectId").toInt();

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

    std::string objectTypeId = manifest.objectTypeIdByInternalName(
        QString::fromStdString(classInfo->internalClassName)).toStdString();

    if (objectTypeId.empty())
        return result;

    result = makePtr<ObjectMetadata>();
    const nx::sdk::Uuid trackId = deviceObjectNumberToUuid(objectId);
    result->setTrackId(trackId);

    result->setTypeId(objectTypeId);

    const Rect relativeRect = scale(*rect, frameScales);
    result->setBoundingBox(relativeRect);

    return result;
}

//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------

Ptr<ObjectMetadataPacket> parseObjectMetadataXml(
    const QByteArray& data, const Hanwha::EngineManifest& manifest)
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

    const auto timestamp = extractTimestamp(frame);
    Ptr<Attribute> timestampAttribute;
    if (timestamp)
    {
        std::chrono::time_point<std::chrono::system_clock> moment =
            QDateTimeToTimePoint(*timestamp);

        std::time_t epoch_time = std::chrono::system_clock::to_time_t(moment);
        std::string t = std::ctime(&epoch_time);

        timestampAttribute = makePtr<Attribute>(nx::sdk::IAttribute::Type::local_time_point,
            "timestamp", timePointToString(moment));
    }
    const QDomElement transformation = getChildElement(frame, "Transformation");
    if (frame.isNull())
        return result;

    auto frameScales = extractScales(transformation);
    if (!frameScales)
        return result;

    result = makePtr<ObjectMetadataPacket>();
    QDomElement object = transformation.nextSiblingElement();
    for (; !object.isNull(); object = object.nextSiblingElement())
    {
        if (Ptr<ObjectMetadata> objectMetadata = extractObject(object, *frameScales, manifest))
        {
            if (timestampAttribute)
                objectMetadata->addAttribute(timestampAttribute);
            result->addItem(objectMetadata.releasePtr());
        }
    }

    //static int i = 0;
    //++i;
    //auto objectMetadataPacket = makePtr<ObjectMetadataPacket>();
    ////const auto ts = dataPacket->timestampUs();
    //objectMetadataPacket->setTimestampUs(timestamp);
    //objectMetadataPacket->setDurationUs(1000'000);

    //auto objectMetadata = makePtr<ObjectMetadata>();
    //static const nx::sdk::Uuid trackId = deviceObjectNumberToUuid(i % 10);
    //objectMetadata->setTrackId(trackId);
    //objectMetadata->setTypeId("nx.hanwha.ObjectDetection.Face");
    //objectMetadata->setBoundingBox(Rect(0.0, 0.0, 0.5, 0.5));

    //objectMetadataPacket->addItem(objectMetadata.get());

    //result.push_back(objectMetadataPacket);
    return result;
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
