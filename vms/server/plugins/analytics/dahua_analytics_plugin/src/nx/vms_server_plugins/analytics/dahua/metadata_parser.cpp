#include "metadata_parser.h"

#include <nx/utils/log/log_main.h>
#include <nx/utils/std/optional.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDateTime>

#include "plugin.h"
#include "engine.h"
#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::dahua {

using namespace std::literals;
using namespace nx::utils;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

constexpr auto kCoordinateDomain = 8192.0f; //< From documentation.
constexpr auto kInstantTrackDuration = 500ms;

QString formatReceivedEventForLog(const Event& event)
{
    QString text = "Received event:";

    text += NX_FMT("\ntypeId: %1", event.type->id);

    text += NX_FMT("\ntimestamp: %1",
        QDateTime::fromMSecsSinceEpoch(event.timestamp.count()).toString(Qt::ISODateWithMs));

    text += NX_FMT("\nisActive: %1", event.isActive);

    if (const auto& name = event.ruleName)
        text += NX_FMT("\nruleName: %1", *name);

    if (const auto& areaId = event.areaId)
        text += NX_FMT("\nareaId: %1", *areaId);

    if (!event.attributes.empty())
    {
        text += "\nattributes:";
        for (const auto& attribute: event.attributes)
            text += NX_FMT("\n\t%1: %2", attribute.name, attribute.value);
    }

    if (!event.objects.empty())
    {
        text += "\nobjects:";
        for (const auto& object: event.objects)
        {
            text += NX_FMT("\n\t%1:", object.id);

            text += NX_FMT("\n\t\ttypeId: %1", object.type->id);

            text += "\n\t\tboundingBox:";
            text += NX_FMT("\n\t\t\tx: %1", object.boundingBox.x);
            text += NX_FMT("\n\t\t\ty: %1", object.boundingBox.y);
            text += NX_FMT("\n\t\t\twidth: %1", object.boundingBox.width);
            text += NX_FMT("\n\t\t\theight: %1", object.boundingBox.height);

            if (!object.attributes.empty())
            {
                text += "\n\t\tattributes:";
                for (const auto& attribute: object.attributes)
                    text += NX_FMT("\n\t\t\t%1: %2", attribute.name, attribute.value);
            }
        }
    }

    return text;
}

std::optional<Object> parseObject(const QJsonObject& jsonObject, const EventType* eventType)
{
    Object object;

    if (const auto value = jsonObject["ObjectType"]; value.isString())
    {
        object.type = ObjectType::findByNativeId(value.toString());
        if (!object.type)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Object has unknown ObjectType: %1", value.toString());
            return std::nullopt;
        }
    }
    else
    {
        NX_DEBUG(NX_SCOPE_TAG, "Object has no ObjectType");
        return std::nullopt;
    }

    if (const auto value = jsonObject["ObjectID"]; value.isDouble())
        object.id = value.toInt();

    if (const auto value = jsonObject["BoundingBox"]; value.isArray())
    {
        if (const auto box = value.toArray(); box.size() == 4)
        {
            const auto domain = eventType->coordinateDomain.value_or(kCoordinateDomain);

            object.boundingBox.x = box[0].toDouble() / domain;
            object.boundingBox.y = box[1].toDouble() / domain;
            object.boundingBox.width = box[2].toDouble() / domain - object.boundingBox.x;
            object.boundingBox.height = box[3].toDouble() / domain - object.boundingBox.y;
        }
    }
    if (!object.boundingBox.isValid())
        return std::nullopt;
    
    if (const auto& parseAttributes = object.type->parseAttributes)
        parseAttributes(&object.attributes, jsonObject);

    return object;
}

std::vector<QJsonObject> defaultExtractObjects(const QJsonObject& eventData)
{
    std::vector<QJsonObject> objects;

    if (const auto objectsValue = eventData["Objects"]; objectsValue.isArray())
    {
        for (const auto objectValue: objectsValue.toArray())
        {
            if (!objectValue.isObject())
                continue;

            objects.push_back(objectValue.toObject());
        }
    }
    else if (const auto objectValue = eventData["Object"]; objectValue.isObject())
    {
        objects.push_back(objectValue.toObject());
    }

    return objects;
}

void parseEventData(Event *event, const QJsonObject& data)
{
    if (const auto value = data["Name"]; value.isString())
        event->ruleName = value.toString();

    if (const auto value = data["AreaID"]; value.isDouble())
        event->areaId = value.toInt();

    if (const auto& parseAttributes = event->type->parseAttributes)
        parseAttributes(&event->attributes, data);

    const auto extractObjects = event->type->extractObjects
        ? event->type->extractObjects
        : defaultExtractObjects;
    for (const auto jsonObject: extractObjects(data))
    {
        if (auto object = parseObject(jsonObject, event->type))
            event->objects.push_back(std::move(*object));
    }
}

} // namespace

MetadataParser::MetadataParser(DeviceAgent* deviceAgent):
    m_deviceAgent(deviceAgent)
{
}

bool MetadataParser::processData(const QnByteArrayConstRef& bytes)
{
    const auto data = QByteArray::fromRawData(bytes.data(), bytes.size());

    if (data != "Heartbeat")
    {
        NX_VERBOSE(this, "Received raw event:\n%1", data);

        parseEvent(data);
    }

    return true;
}

void MetadataParser::setNeededTypes(const Ptr<const IMetadataTypes>& types)
{
    m_neededEventTypes.clear();

    if (!types)
        return;

    const auto eventTypeIds = types->eventTypeIds();
    if (!NX_ASSERT(eventTypeIds))
        return;

    for (int i = 0; i < eventTypeIds->count(); ++i)
    {
        const QString id = eventTypeIds->at(i);

        const auto type = EventType::findById(id);
        if (!NX_ASSERT(type))
            continue;

        m_neededEventTypes.insert(type);
    }
}

void MetadataParser::terminateOngoingEvents()
{
    for (auto it = m_ongoingEvents.begin(); it != m_ongoingEvents.end(); )
    {
        Event event = (it++)->second;

        event.isActive = false;
        event.timestamp = m_deviceAgent->engine()->plugin()->vmsSystemTimeSinceEpoch();

        process(event);
    }
}

void MetadataParser::parseEvent(const QByteArray& data)
{
    // Example data:
    //
    //     Code=FaceDetection;action=Start;index=0;data={
    //        "CfgRuleId" : 2,
    //        "Class" : "FaceDetection",
    //        "CountInGroup" : 2,
    //        "DetectRegion" : null,
    //        "EventID" : 10003,
    //        "EventSeq" : 2,
    //        "Faces" : [
    //           {
    //              "BoundingBox" : [ 5656, 112, 6496, 1608 ],
    //              "Center" : [ 6072, 856 ],
    //              "ObjectID" : 1,
    //              "ObjectType" : "HumanFace",
    //              "RelativeID" : 0
    //           }
    //        ],
    //        "FrameSequence" : 1514,
    //        "GroupID" : 2,
    //        "Mark" : 0,
    //        "Name" : "FaceDetection",
    //        "Object" : {
    //           "Action" : "Appear",
    //           "BoundingBox" : [ 5656, 112, 6496, 1608 ],
    //           "Center" : [ 6072, 856 ],
    //           "Confidence" : 19,
    //           "FrameSequence" : 1514,
    //           "ObjectID" : 1,
    //           "ObjectType" : "HumanFace",
    //           "RelativeID" : 0,
    //           "SerialUUID" : "",
    //           "Source" : 0.0,
    //           "Speed" : 0,
    //           "SpeedTypeInternal" : 0
    //        },
    //        "Objects" : [
    //           {
    //              "Action" : "Appear",
    //              "BoundingBox" : [ 5656, 112, 6496, 1608 ],
    //              "Center" : [ 6072, 856 ],
    //              "Confidence" : 19,
    //              "FrameSequence" : 1514,
    //              "ObjectID" : 1,
    //              "ObjectType" : "HumanFace",
    //              "RelativeID" : 0,
    //              "SerialUUID" : "",
    //              "Source" : 0.0,
    //              "Speed" : 0,
    //              "SpeedTypeInternal" : 0
    //           }
    //        ],
    //        "PTS" : 42949532630.0,
    //        "Priority" : 0,
    //        "RuleID" : 2,
    //        "RuleId" : 1,
    //        "Source" : 2049281960.0,
    //        "UTC" : 1606416265,
    //        "UTCMS" : 0
    //     }

    Event event;

    event.timestamp = m_deviceAgent->engine()->plugin()->vmsSystemTimeSinceEpoch();

    int offset = 0;
    while (offset < data.size())
    {
        const int keyEnd = data.indexOf('=', offset);
        if (keyEnd == -1)
        {
            NX_DEBUG(this, "Event is malformed");
            return;
        }
        const auto key = data.mid(offset, keyEnd - offset);

        offset = keyEnd + 1;

        int valueEnd;
        if (key != "data")
        {
            valueEnd = data.indexOf(';', offset);
            if (valueEnd == -1)
                valueEnd = data.size();
        }
        else
        {
            valueEnd = data.size();
        }

        const auto value = data.mid(offset, valueEnd - offset);
        if (key == "Code")
        {
            event.type = EventType::findByNativeId(QString::fromUtf8(value));
            if (!event.type)
            {
                NX_DEBUG(this, "Event has unknown Code: %1", value);
                return;
            }
            if (!m_neededEventTypes.count(event.type))
                return;
        }
        else if (key == "action")
        {
            event.isActive = (value == "Start" || value == "Pulse");
        }
        else if (key == "data")
        {
            if (event.type)
                parseEventData(&event, QJsonDocument::fromJson(value).object());
        }

        offset = valueEnd + 1;
    }

    if (!event.type)
    {
        NX_DEBUG(this, "Event has no Code");
        return;
    }

    NX_VERBOSE(this, formatReceivedEventForLog(event));
    
    process(event);
}

void MetadataParser::process(const Event& event)
{
    if (event.type->isStateDependent)
    {
        const auto [it, isEmplaced] = m_ongoingEvents.try_emplace(event.type);
        auto& ongoingEvent = it->second;

        static_cast<Event&>(ongoingEvent) = event;

        if (isEmplaced)
        {
            Uuid trackId;
            for (const auto& object: event.objects)
            {
                trackId = UuidHelper::randomUuid();
                emitObject(object, trackId, event);
            }

            if (event.objects.size() == 1)
            {
                ongoingEvent.singleCausingObject = event.objects[0];
                ongoingEvent.trackId = trackId;
            }
            else
            {
                ongoingEvent.trackId = UuidHelper::randomUuid();
            }
        }

        if (isEmplaced == event.isActive)
            emitEvent(event, &ongoingEvent.trackId, getIf(&ongoingEvent.singleCausingObject));

        if (!event.isActive)
            m_ongoingEvents.erase(it);
    }
    else
    {
        Uuid trackId;
        for (const auto& object: event.objects)
        {
            trackId = UuidHelper::randomUuid();
            emitObject(object, trackId, event);
        }

        if (event.objects.size() == 1)
            emitEvent(event, &trackId, &event.objects[0]);
        else
            emitEvent(event, nullptr, nullptr);
    }
}

void MetadataParser::emitObject(const Object& object, const Uuid& trackId, const Event& event)
{
    const auto packet = makePtr<ObjectMetadataPacket>();
    const auto metadata = makePtr<ObjectMetadata>();

    metadata->setTypeId(object.type->id.toStdString());
    metadata->setTrackId(trackId);
    metadata->setBoundingBox(object.boundingBox);

    packet->setTimestampUs(
        std::chrono::duration_cast<std::chrono::microseconds>(event.timestamp).count());
    packet->setDurationUs(
        std::chrono::duration_cast<std::chrono::microseconds>(kInstantTrackDuration).count());

    for (const auto& [name, value, type]: object.attributes)
    {
        metadata->addAttribute(
            makePtr<sdk::Attribute>(type, name.toStdString(), value.toStdString()));
    }

    packet->addItem(metadata.get());
    emitMetadataPacket(packet);

    const auto bestShotPacket = makePtr<ObjectTrackBestShotPacket>();
    bestShotPacket->setTrackId(trackId);
    bestShotPacket->setBoundingBox(object.boundingBox);
    bestShotPacket->setTimestampUs(
        std::chrono::duration_cast<std::chrono::microseconds>(event.timestamp).count());
    emitMetadataPacket(bestShotPacket);
}

void MetadataParser::emitEvent(
    const Event& event, const Uuid* trackId, const Object* singleCausingObject)
{
    const auto packet = makePtr<EventMetadataPacket>();
    const auto metadata = makePtr<EventMetadata>();

    metadata->setTypeId(event.type->id.toStdString());

    if (trackId)
        metadata->setTrackId(*trackId);

    metadata->setIsActive(event.isActive);

    metadata->setCaption(event.type->prettyName.toStdString());

    QString description = event.type->description;
    description = NX_FMT("%1 has %2", description, event.isActive ? "been detected" : "ended");
    if (event.areaId)
        description = NX_FMT("%1 in region %2", description, *event.areaId);
    if (event.ruleName)
        description = NX_FMT("%1 by rule %2", description, *event.ruleName);
    metadata->setDescription(description.toStdString());

    packet->setTimestampUs(
        std::chrono::duration_cast<std::chrono::microseconds>(event.timestamp).count());

    for (const auto& [name, value, type]: event.attributes)
    {
        metadata->addAttribute(
            makePtr<sdk::Attribute>(type, name.toStdString(), value.toStdString()));
    }
    
    if (singleCausingObject)
    {
        for (const auto& [name, value, type]: singleCausingObject->attributes)
        {
            metadata->addAttribute(
                makePtr<sdk::Attribute>(type, name.toStdString(), value.toStdString()));
        }
    }

    packet->addItem(metadata.get());
    emitMetadataPacket(packet);
}

void MetadataParser::emitMetadataPacket(const Ptr<IMetadataPacket>& packet)
{
    const auto& handler = m_deviceAgent->handler();
    handler->handleMetadata(packet.get());
}

} // namespace nx::vms_server_plugins::analytics::dahua
