#include "metadata_parser.h"

#include <nx/utils/log/log_main.h>
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

#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::dahua {

using namespace std::literals;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

const auto kCoordinateDomain = 8192.0f;
const auto kOngoingEventTimeout = 5s;
const auto kInstantTrackDuration = 500ms;

QString formatReceivedEventForLog(const Event& event)
{
    QString text = "Received event:";

    text += NX_FMT("\ntypeId: %1", event.type->id);

    if (const auto& sequenceId = event.sequenceId)
        text += NX_FMT("\nsequenceId: %1", *sequenceId);

    if (const auto& timestamp = event.timestamp)
    {
        text += NX_FMT("\ntimestamp: %1",
            QDateTime::fromMSecsSinceEpoch(timestamp->count()).toString(Qt::ISODateWithMs));
    }

    text += NX_FMT("\nisActive: %1", event.isActive);

    if (const auto& name = event.ruleName)
        text += NX_FMT("\nruleName: %1", *name);

    if (const auto& object = event.object)
    {
        text += "\nobject:";

        text += NX_FMT("\n\ttypeId: %1", object->type->id);

        text += "\n\tboundingBox:";
        text += NX_FMT("\n\t\tx: %1", object->boundingBox.x);
        text += NX_FMT("\n\t\ty: %1", object->boundingBox.y);
        text += NX_FMT("\n\t\twidth: %1", object->boundingBox.width);
        text += NX_FMT("\n\t\theight: %1", object->boundingBox.height);

        if (!object->attributes.empty())
        {
            text += "\n\tattributes:";
            for (const auto& attribute: object->attributes)
                text += NX_FMT("\n\t\t%1: %2", attribute.name, attribute.value);
        }
    }

    return text;
}

std::optional<Object> parseObject(const QJsonObject& jsonObject)
{
    Object object;

    if (const auto type = jsonObject["ObjectType"]; type.isString())
    {
        object.type = ObjectType::findByNativeId(type.toString());
        if (!object.type)
            return std::nullopt;
    }

    if (const auto box = jsonObject["BoundingBox"].toArray(); box.size() == 4)
    {
        object.boundingBox.x = box[0].toDouble() / kCoordinateDomain;
        object.boundingBox.y = box[1].toDouble() / kCoordinateDomain;
        object.boundingBox.width = box[2].toDouble() / kCoordinateDomain - object.boundingBox.x;
        object.boundingBox.height = box[3].toDouble() / kCoordinateDomain - object.boundingBox.y;
    }
    
    if (const auto& parseAttributes = object.type->parseAttributes)
        object.attributes = parseAttributes(jsonObject);

    return object;
}

void parseEventData(Event *event, const QJsonObject& data)
{
    if (const auto utc = data["UTC"]; utc.isDouble())
    {
        event->timestamp = std::chrono::seconds(utc.toInt())
            + std::chrono::milliseconds(data["UTCMS"].toInt());
    }

    if (const auto eventSeq = data["EventSeq"]; eventSeq.isDouble())
        event->sequenceId = eventSeq.toInt();

    if (const auto name = data["Name"]; name.isString())
        event->ruleName = name.toString();

    if (auto object = data["Object"].toObject(); !object.isEmpty())
        event->object = parseObject(object);
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

void MetadataParser::terminateOngoingEvents()
{
    m_basicPollable.executeInAioThreadSync(
        [&]()
        {
            for (auto it = m_ongoingEvents.begin(); it != m_ongoingEvents.end(); )
                terminate((it++)->second);
        });
}

MetadataParser::OngoingEvent::Key MetadataParser::getKey(const Event& event)
{
    return {event.type, event.sequenceId};
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
        }
        else if (key == "action")
        {
            event.isActive = (value == "Start" || value == "Pulse");
        }
        else if (key == "data")
        {
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
    
    m_basicPollable.executeInAioThreadSync(
        [&]()
        {
            process(event);
        });
}

void MetadataParser::process(const Event& event)
{
    Uuid trackId;
    bool eventShouldBeEmitted = false;
    if (event.type->isStateDependent)
    {
        bool isFirst = false;
        bool isLast = false;

        const auto key = getKey(event);
        auto& ongoingEvent = m_ongoingEvents[key];
        if (ongoingEvent.trackId.isNull())
        {
            ongoingEvent.trackId = UuidHelper::randomUuid();
            ongoingEvent.timer.bindToAioThread(m_basicPollable.getAioThread());
            isFirst = true;
        }
        
        if (event.isActive)
        {
            ongoingEvent.Event::operator=(event);
            ongoingEvent.sinceLastUpdate.restart();
            ongoingEvent.timer.start(kOngoingEventTimeout,
                [this, key]() mutable
                {
                    const auto it = m_ongoingEvents.find(key);
                    if (it == m_ongoingEvents.end())
                        return;

                    terminate(it->second);
                });
        }
        else
        {
            ongoingEvent.timer.pleaseStopSync();
            m_ongoingEvents.erase(key);
            isLast = true;
        }

        trackId = ongoingEvent.trackId;
        eventShouldBeEmitted = (isFirst != isLast);
    }
    else
    {
        trackId = UuidHelper::randomUuid();
        eventShouldBeEmitted = true;
    }

    emitObject(event, trackId);
    if (eventShouldBeEmitted)
        emitEvent(event, trackId);
}

void MetadataParser::emitObject(const Event& event, const nx::sdk::Uuid& trackId)
{
    if (!event.object)
        return;

    auto& object = *event.object;

    const auto packet = makePtr<ObjectMetadataPacket>();
    const auto metadata = makePtr<ObjectMetadata>();

    metadata->setTypeId(object.type->id.toStdString());
    metadata->setTrackId(trackId);
    metadata->setBoundingBox(object.boundingBox);

    if (event.timestamp)
    {
        packet->setTimestampUs(
            std::chrono::duration_cast<std::chrono::microseconds>(*event.timestamp).count());
        packet->setFlags(ObjectMetadataPacket::Flags::cameraClockTimestamp);
    }
    packet->setDurationUs(
        std::chrono::duration_cast<std::chrono::microseconds>(
            event.isActive ? kInstantTrackDuration : 1us).count());

    for (const auto& [name, value, type]: object.attributes)
    {
        metadata->addAttribute(
            makePtr<Attribute>(type, name.toStdString(), value.toStdString()));
    }

    packet->addItem(metadata.get());
    emitMetadataPacket(packet);

    const auto bestShotPacket = makePtr<ObjectTrackBestShotPacket>();
    bestShotPacket->setTrackId(trackId);
    bestShotPacket->setBoundingBox(object.boundingBox);
    if (event.timestamp)
    {
        bestShotPacket->setTimestampUs(
            std::chrono::duration_cast<std::chrono::microseconds>(*event.timestamp).count());
        bestShotPacket->setFlags(ObjectMetadataPacket::Flags::cameraClockTimestamp);
    }
    emitMetadataPacket(bestShotPacket);
}

void MetadataParser::emitEvent(const Event& event, const nx::sdk::Uuid& trackId)
{
    const auto packet = makePtr<EventMetadataPacket>();
    const auto metadata = makePtr<EventMetadata>();

    metadata->setTypeId(event.type->id.toStdString());
    metadata->setTrackId(trackId);
    metadata->setIsActive(event.isActive);

    metadata->setCaption(event.type->prettyName.toStdString());

    QString description = event.type->description;
    if (event.type->isRegionDependent && event.ruleName)
        description = NX_FMT("%1 by rule %2", description, *event.ruleName);
    if (event.type->isStateDependent)
        description = NX_FMT("%1 has %2", description, event.isActive ? "been detected" : "ended");
    metadata->setDescription(description.toStdString());

    if (event.timestamp)
    {
        packet->setTimestampUs(
            std::chrono::duration_cast<std::chrono::microseconds>(*event.timestamp).count());

        packet->setFlags(EventMetadataPacket::Flags::cameraClockTimestamp);
    }
    
    if (const auto& object = event.object)
    {
        for (const auto& [name, value, type]: object->attributes)
        {
            metadata->addAttribute(
                makePtr<Attribute>(type, name.toStdString(), value.toStdString()));
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

void MetadataParser::terminate(const OngoingEvent& ongoingEvent)
{
    Event event = ongoingEvent;
    event.isActive = false;
    if (event.timestamp)
        *event.timestamp += ongoingEvent.sinceLastUpdate.elapsed();
    process(event);
}

} // namespace nx::vms_server_plugins::analytics::dahua
