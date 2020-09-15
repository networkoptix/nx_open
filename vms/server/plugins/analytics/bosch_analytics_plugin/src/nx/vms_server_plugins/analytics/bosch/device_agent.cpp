// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <QtCore/QByteArray>

#include <chrono>
#include <iostream>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/uuid.h>

#include <nx/utils/log/assert.h>

#include "metadata_xml_parser.h"
//#include "xml_examples_ut.h"

namespace nx::vms_server_plugins::analytics::bosch {

namespace {

[[nodiscard]] QString ExtractEventTypeNameFromTopic(const QString& oasisTopic)
{
    QString qualifiedEventName = oasisTopic.split('/').back();
    return qualifiedEventName.split(':').back();
}

Ptr<EventMetadata> parsedEventToEventMetadata(
    const ParsedEvent& event, const Bosch::EngineManifest& manifest)
{
    const QString internalName = ExtractEventTypeNameFromTopic(event.topic);
    const Bosch::EventType* eventType =
        manifest.eventTypeByInternalName(internalName);
    if (!eventType)
        return {};

    Ptr<EventMetadata> eventMetadata = nx::sdk::makePtr<EventMetadata>();
    eventMetadata->setTypeId(eventType->id.toStdString());
    eventMetadata->setCaption(eventType->name.toStdString());

    eventMetadata->setDescription(eventType->fullDescription().toStdString());
    eventMetadata->setIsActive(event.isActive);
    eventMetadata->setConfidence(1.0);
    return eventMetadata;
}

nx::sdk::analytics::Rect parsedRectToObjectMetadataRect(Rect parsedRect)
{
    // parsedRect coordinates: [-1, 1] (y axis goes up)
    // objectMatadata coordinated: [0, 1] (y axis goes down)

    nx::sdk::analytics::Rect result;
    result.x = (parsedRect.left + 1.0) / 2;
    result.y = 1.0 - (parsedRect.top + 1.0) / 2;
    result.width = (parsedRect.right - parsedRect.left) / 2;
    result.height = (parsedRect.top - parsedRect.bottom) / 2;
    return result;
}

Ptr<ObjectMetadata> parsedObjectToObjectMetadata(
    const ParsedObject& object, const Bosch::EngineManifest& /*manifest*/, UuidCache* uuidCache)
{
    Ptr<ObjectMetadata> objectMetadata = nx::sdk::makePtr<ObjectMetadata>();
    objectMetadata->setTypeId(DeviceAgent::kObjectDetectionObjectTypeId.toStdString());

    objectMetadata->setTrackId(uuidCache->makeUuid(object.id));

    objectMetadata->setBoundingBox(parsedRectToObjectMetadataRect(object.shape.boundingBox));
    objectMetadata->addAttribute(makePtr<nx::sdk::Attribute>(
        nx::sdk::IAttribute::Type::string,
        "velocity",
        std::to_string(object.velocity)));

    objectMetadata->setConfidence(1.0);
    return objectMetadata;
}

} // namespace

//-------------------------------------------------------------------------------------------------

void DeviceInfo::init(const IDeviceInfo* deviceInfo)
{
    url = deviceInfo->url();
    model = deviceInfo->model();
    firmware = deviceInfo->firmware();
    auth.setUser(deviceInfo->login());
    auth.setPassword(deviceInfo->password());
    uniqueId = deviceInfo->id();
    sharedId = deviceInfo->sharedId();
    channelNumber = deviceInfo->channelNumber();
}

/**
 * @param deviceInfo Various information about the related device, such as its id, vendor, model,
 *     etc.
 */
DeviceAgent::DeviceAgent(Engine* engine, const IDeviceInfo* deviceInfo): m_engine(engine)
{
    m_deviceInfo.init(deviceInfo);
    buildInitialManifest();
}

DeviceAgent::~DeviceAgent()
{
}

//-------------------------------------------------------------------------------------------------
/**
 * Called before other methods. Server provides the set of settings stored in its database,
 * combined with the values received from the plugin via pluginSideSettings() (if any), for
 * the combination of a device instance and an Engine instance.
 */
/*virtual*/ void DeviceAgent::doSetSettings(
    Result<const ISettingsResponse*>* outResult, const IStringMap* settings) /*override*/
{

}

/**
 * In addition to the settings stored in a Server database, a DeviceAgent can have some
 * settings which are stored somewhere "under the hood" of the DeviceAgent, e.g. on a device
 * acting as a DeviceAgent's backend. Such settings do not need to be explicitly marked in the
 * Settings Model, but every time the Server offers the user to edit the values, it calls this
 * method and merges the received values with the ones in its database.
 */
/*virtual*/ void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* outResult) const /*override*/
{

}

/** Provides DeviceAgent manifest in JSON format. */
/*virtual*/ void DeviceAgent::getManifest(Result<const IString*>* outResult) const /*override*/
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    *outResult = new nx::sdk::String(QJson::serialized(m_manifest));
}

/**
 * @param handler Processes event metadata and object metadata fetched by DeviceAgent.
 *     DeviceAgent should fetch events metadata after setNeededMetadataTypes() call.
 *     Generic device related events (errors, warning, info messages) might also be reported
 *     via this handler.
 */
/*virtual*/ void DeviceAgent::setHandler(
    nx::sdk::analytics::IDeviceAgent::IHandler* handler) /*override*/
{
    handler->addRef();
    m_handler.reset(handler);
}

/**
 * Sets a list of metadata types that are needed by the Server. Empty list means that the
 * Server does not need any metadata from this DeviceAgent.
 */
/*virtual*/ void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/) /*override*/
{
}

Ptr<EventMetadataPacket> DeviceAgent::buildEventPacket(
    const ParsedMetadata& parsedMetadata, int64_t ts) const
{
    using namespace std::chrono;

    Ptr<EventMetadataPacket> packet;
    if (parsedMetadata.events.isEmpty())
        return packet;

    packet = nx::sdk::makePtr<EventMetadataPacket>();
    for (const ParsedEvent& event: parsedMetadata.events)
    {
        Ptr<EventMetadata> eventMetadata = parsedEventToEventMetadata(event, m_engine->manifest());
        if (eventMetadata)
            packet->addItem(eventMetadata.get());
    }
    if (packet->count() == 0)
    {
        packet.reset();
        return packet;
    }

    packet->setTimestampUs(ts);
    packet->setDurationUs(1'000'000);
    return packet;
}

Ptr<EventMetadataPacket> DeviceAgent::buildObjectDetectionEventPacket(int64_t ts) const
{
    using namespace std::chrono;

    Ptr<EventMetadataPacket> packet = nx::sdk::makePtr<EventMetadataPacket>();

    const Bosch::EventType* eventType =
        m_engine->manifest().eventTypeByInternalName(kObjectDetectionEventTypeId.split('.').back());
    if (!eventType)
        return {};

    Ptr<EventMetadata> eventMetadata = nx::sdk::makePtr<EventMetadata>();
    eventMetadata->setTypeId(kObjectDetectionEventTypeId.toStdString());
    eventMetadata->setCaption(eventType->name.toStdString());

    eventMetadata->setDescription(eventType->fullDescription().toStdString());
    eventMetadata->setIsActive(true);
    eventMetadata->setConfidence(1.0);

    if (eventMetadata)
        packet->addItem(eventMetadata.get());

   if (packet->count() == 0)
    {
        packet.reset();
        return packet;
    }

    packet->setTimestampUs(ts);
    packet->setDurationUs(1'000'000);
    return packet;
}

Ptr<ObjectMetadataPacket> DeviceAgent::buildObjectPacket(
    const ParsedMetadata& parsedMetadata, int64_t ts) const
{
    using namespace std::chrono;

    Ptr<ObjectMetadataPacket> packet;
    if (parsedMetadata.objects.isEmpty())
        return packet;

    packet = nx::sdk::makePtr<ObjectMetadataPacket>();
    for (const ParsedObject& object: parsedMetadata.objects)
    {
        Ptr<ObjectMetadata> objectMetadata = parsedObjectToObjectMetadata(
            object, m_engine->manifest(), &m_uuidCache);

        if (objectMetadata)
            packet->addItem(objectMetadata.get());
    }
    if (packet->count() == 0)
    {
        packet.reset();
        return packet;
    }

    packet->setTimestampUs(ts);
    packet->setDurationUs(1'000'000);
    return packet;
}

int DeviceAgent::buildInitialManifest()
{
    m_manifest = {};
    m_wsntTopics.clear();

    m_manifest.capabilities.setFlag(
        nx::vms::api::analytics::DeviceAgentManifest::disableStreamSelection);

    const auto& engineEventTypes = m_engine->manifest().eventTypes;

    // Add all event types that can not be autodetected.
    for (const auto& eventType: engineEventTypes)
    {
        if (!eventType.autodetect)
        {
            m_manifest.supportedEventTypeIds << eventType.id;
            m_wsntTopics.insert(eventType.internalName);
        }
    }
    m_manifest.supportedEventTypeIds.sort(); //< for convenient view in debugger

    // Add the only object type Bosch cameras understand.
    m_manifest.supportedObjectTypeIds << kObjectDetectionObjectTypeId;

    return m_manifest.supportedEventTypeIds.size();
}

void DeviceAgent::updateAgentManifest(const ParsedMetadata& parsedMetadata)
{
    // Currently for Bosch cameras there's no mechanism to detect when an eventType is deactivated
    // on a camera, we can only detect when new eventTypes appears, so this function only adds
    // eventTypes.

    const auto& engineEventTypes = m_engine->manifest().eventTypes;
    bool manifestUpdated = false;
    for (const ParsedEvent& parsedEvent: parsedMetadata.events)
    {
        const QString topic = ExtractEventTypeNameFromTopic(parsedEvent.topic);
        if (!m_wsntTopics.contains(topic))
        {
            m_wsntTopics.insert(topic);
            auto it = engineEventTypes.find(topic);
            if (it == engineEventTypes.end())
            {
                NX_DEBUG(this,
                    "Unknown wsnt topic (event type internal name) received in notification xml: %1",
                    parsedEvent.topic);
            }
            else
            {
                m_manifest.supportedEventTypeIds << it->id;
                NX_DEBUG(this,
                    "New wsnt topic (event type internal name) added: %1",
                    parsedEvent.topic);
                manifestUpdated = true;
            }
        }
    }
    if (manifestUpdated)
        m_handler->pushManifest(new nx::sdk::String(QJson::serialized(m_manifest)));
}

/*virtual*/ void DeviceAgent::doPushDataPacket(
    Result<void>* /*outResult*/, IDataPacket* dataPacket) /*override*/
{
    const auto incomingPacket = dataPacket->queryInterface<ICustomMetadataPacket>();
    const QByteArray xmlData(incomingPacket->data(), incomingPacket->dataSize());
    MetadataXmlParser parser(xmlData);
    const ParsedMetadata parsedMetadata = parser.parse();
    updateAgentManifest(parsedMetadata);

    Ptr<EventMetadataPacket> eventPacket =
        buildEventPacket(parsedMetadata, dataPacket->timestampUs());
    if (static int i = 0; eventPacket && NX_ASSERT(m_handler))
    {
        std::cout << std::endl
            << i
            << "  ===EVENTS======================================================================="
            << std::endl;
        for (const auto& event : parsedMetadata.events)
            std::cout << event.topic.toStdString() << " isActive = " << event.isActive << std::endl;

        m_handler->handleMetadata(eventPacket.get());

        ++i;
    }

    Ptr<ObjectMetadataPacket> objectPacket =
        buildObjectPacket(parsedMetadata, dataPacket->timestampUs());
    if (static int i = 0; objectPacket && NX_ASSERT(m_handler))
    {
        std::cout << std::endl
            << i
            << "  ---OBJECTS----------------------------------------------------------------------"
            << std::endl;

        for (const auto& object: parsedMetadata.objects)
            std::cout << "Id = " << object.id << std::endl;

        m_handler->handleMetadata(objectPacket.get());

        for (const auto& object : parsedMetadata.objects)
        {
            if (!m_idCache.alreadyContains(object.id))
            {
                Ptr<EventMetadataPacket> auxEventPacket =
                    buildObjectDetectionEventPacket(dataPacket->timestampUs());
                if (auxEventPacket)
                {
                    m_handler->handleMetadata(auxEventPacket.get());
                    std::cout << "event sent for id = " << object.id << std::endl;
                }
            }
        }
        ++i;
    }

}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::bosch
