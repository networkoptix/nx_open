#include "metadata_parser.h"

#include <nx/utils/log/log_main.h>
#include <nx/utils/std/optional.h>
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

std::optional<Object> parseObject(const QJsonObject& jsonObject)
{
    Object object;

    if (const auto type = jsonObject["ObjectType"]; type.isString())
    {
        object.type = ObjectType::findByNativeId(type.toString());
        if (!object.type)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Object has unknown ObjectType: %1", type.toString());
            return std::nullopt;
        }
    }
    else
    {
        NX_DEBUG(NX_SCOPE_TAG, "Object has no ObjectType");
        return std::nullopt;
    }

    if (const auto id = jsonObject["ObjectID"]; id.isDouble())
        object.id = id.toInt();

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
    if (const auto eventSeq = data["EventSeq"]; eventSeq.isDouble())
        event->sequenceId = eventSeq.toInt();

    if (const auto name = data["Name"]; name.isString())
        event->ruleName = name.toString();

    if (const auto objects = data["Objects"].toArray(); !objects.isEmpty())
    {
        for (int i = 0; i < objects.size(); ++i)
        {
            if (auto object = parseObject(objects[i].toObject()))
                event->objects.push_back(std::move(*object));
        }
    }
    else if (const auto jsonObject = data["Object"].toObject(); !jsonObject.isEmpty())
    {
        if (auto object = parseObject(jsonObject))
            event->objects.push_back(std::move(*object));
    }
}

} // namespace

MetadataParser::MetadataParser(DeviceAgent* deviceAgent):
    m_deviceAgent(deviceAgent),
    m_utilityProvider(deviceAgent->engine()->plugin()->utilityProvider())
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

Uuid MetadataParser::ObjectIdToTrackIdMap::operator()(const decltype(Object::id)& objectId)
{
    const auto it = m_entries.try_emplace(objectId).first;
    auto& entry = it->second;

    entry.sinceLastAccess.restart();
    cleanUpStaleEntries();

    return entry.trackId;
}

void MetadataParser::ObjectIdToTrackIdMap::cleanUpStaleEntries()
{
    constexpr auto kCleanupInterval = 30s;
    if (!m_sinceLastCleanup.hasExpired(kCleanupInterval))
        return;

    for (auto it = m_entries.begin(); it != m_entries.end(); )
    {
        const auto& entry = it->second;

        constexpr auto kEntryTimeout = 5min;
        if (entry.sinceLastAccess.hasExpired(kEntryTimeout))
            it = m_entries.erase(it);
        else
            ++it;
    }

    m_sinceLastCleanup.restart();
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

    event.timestamp = std::chrono::milliseconds(m_utilityProvider->vmsSystemTimeSinceEpochMs());

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
    if (event.type->isStateDependent)
    {
        const auto key = getKey(event);
        const auto [it, isEmplaced] = m_ongoingEvents.try_emplace(key);
        auto& ongoingEvent = it->second;

        if (isEmplaced)
            ongoingEvent.timer.bindToAioThread(m_basicPollable.getAioThread());

        if (!event.objects.empty())
        {
            const auto& newPrimaryObject = *std::min_element(
                event.objects.begin(), event.objects.end(),
                [](const auto& a, const auto& b) { return a.id < b.id; });

            if (auto& primaryObject = ongoingEvent.primaryObject;
                !primaryObject || primaryObject->id == newPrimaryObject.id)
            {
                primaryObject = newPrimaryObject;
            }
        }
        
        bool isErased = false;
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
            m_ongoingEvents.erase(it);
            isErased = true;
        }

        emitObjects(event);
        if (isEmplaced != isErased)
            emitEvent(event, getIf(&ongoingEvent.primaryObject));
    }
    else
    {
        emitObjects(event);
        emitEvent(event);
    }
}

void MetadataParser::emitObjects(const Event& event)
{
    for (const auto& object: event.objects)
    {
        const auto trackId = m_objectIdToTrackIdMap(object.id);

        const auto packet = makePtr<ObjectMetadataPacket>();
        const auto metadata = makePtr<ObjectMetadata>();

        metadata->setTypeId(object.type->id.toStdString());
        metadata->setTrackId(trackId);
        metadata->setBoundingBox(object.boundingBox);

        if (event.timestamp)
        {
            packet->setTimestampUs(
                std::chrono::duration_cast<std::chrono::microseconds>(*event.timestamp).count());
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
        }
        emitMetadataPacket(bestShotPacket);
    }
}

void MetadataParser::emitEvent(const Event& event, const Object* primaryObject)
{
    const auto packet = makePtr<EventMetadataPacket>();
    const auto metadata = makePtr<EventMetadata>();

    metadata->setTypeId(event.type->id.toStdString());

    if (primaryObject)
        metadata->setTrackId(m_objectIdToTrackIdMap(primaryObject->id));

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
    }
    
    if (primaryObject)
    {
        for (const auto& [name, value, type]: primaryObject->attributes)
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
