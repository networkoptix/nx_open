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

template<class T>
nx::sdk::Uuid deviceObjectNumberToUuid(T deviceObjectNumber)
{
    nx::sdk::Uuid serverObjectNumber;
    memcpy(&serverObjectNumber, &deviceObjectNumber,
        std::min(sizeof(serverObjectNumber), sizeof(deviceObjectNumber)));
    return serverObjectNumber;
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
    // parsedRect coordinates: [-1, 1]
    // objectMatadata coordinated: [0, 1]

    nx::sdk::analytics::Rect result;
    result.x = (parsedRect.left + 1.0) / 2;
    result.y = (parsedRect.top + 1.0) / 2;
    result.width = (parsedRect.right - parsedRect.left) / 2;
    result.height = (parsedRect.top - parsedRect.bottom) / 2;
    return result;
}

Ptr<ObjectMetadata> parsedObjectToObjectMetadata(
    const ParsedObject& object, const Bosch::EngineManifest& /*manifest*/)
{
    Ptr<ObjectMetadata> objectMetadata = nx::sdk::makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.bosch.ObjectDetection.AnyObject");

    objectMetadata->setTrackId(deviceObjectNumberToUuid(object.id));

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
    this->m_deviceInfo.init(deviceInfo);

    //m_manifest.capabilities.setFlag(
    //    nx::vms::api::analytics::DeviceAgentManifest::disableStreamSelection);

    //m_manifest.supportedEventTypeIds
    //    << "nx.bosch.GlobalChange"
    //    << "nx.bosch.SignalTooBright"
    //    << "nx.bosch.SignalTooDark"
    //    << "nx.bosch.SignalTooBlurry"
    //    << "nx.bosch.SignalLoss"
    //    << "nx.bosch.FlameDetected"
    //    << "nx.bosch.SmokeDetected"
    //    << "nx.bosch.MotionAlarm"
    //    << "nx.bosch.Detect_any_object"
    //    ;

    //m_manifest.supportedObjectTypeIds << "nx.bosch.ObjectDetection.AnyObject";
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
    // Initially we do not know which event types are supported, we'll get them in the first
    // metadata packet. So 'agentManifest.supportedEventTypeIds' is empty.
    // Nevertheless the manifest should be provided here, or metadata packets will no arrive to
    // the plugin.

    Bosch::DeviceAgentManifest agentManifest;
    agentManifest.capabilities.setFlag(
        nx::vms::api::analytics::DeviceAgentManifest::disableStreamSelection);

    // For debug purposes uncomment the following code to add all potentially possible event types.
#if 0
    agentManifest.supportedEventTypeIds
    << "nx.bosch.GlobalChange"
    << "nx.bosch.SignalTooBright"
    << "nx.bosch.SignalTooDark"
    << "nx.bosch.SignalTooBlurry"
    << "nx.bosch.SignalLoss"
    << "nx.bosch.FlameDetected"
    << "nx.bosch.SmokeDetected"
    << "nx.bosch.MotionAlarm"
    << "nx.bosch.Detect_any_object"
    ;
#endif

    agentManifest.supportedObjectTypeIds << "nx.bosch.ObjectDetection.AnyObject";
    *outResult = new nx::sdk::String(QJson::serialized(agentManifest));

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

Ptr<EventMetadataPacket> DeviceAgent::buildEventPacket(const ParsedMetadata& parsedMetadata, int64_t ts) const
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

    packet->setTimestampUs(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUs(-1);
    return packet;
}

Ptr<ObjectMetadataPacket> DeviceAgent::buildObjectPacket(const ParsedMetadata& parsedMetadata, int64_t ts) const
{
    using namespace std::chrono;

    Ptr<ObjectMetadataPacket> packet;
    if (parsedMetadata.objects.isEmpty())
        return packet;

    packet = nx::sdk::makePtr<ObjectMetadataPacket>();
    for (const ParsedObject& object: parsedMetadata.objects)
    {
        Ptr<ObjectMetadata> objectMetadata = parsedObjectToObjectMetadata(object, m_engine->manifest());
        if (objectMetadata)
            packet->addItem(objectMetadata.get());
    }
    if (packet->count() == 0)
    {
        packet.reset();
        return packet;
    }

    packet->setTimestampUs(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUs(-1);
    return packet;
}

bool DeviceAgent::replanishSupportedEventTypeIds(const ParsedMetadata& parsedMetadata)
{
    int oldSize = m_wsntTopics.size();
    for (const ParsedEvent& id: parsedMetadata.events)
        m_wsntTopics.insert(id.topic);

    return oldSize < m_wsntTopics.size();
}

void DeviceAgent::updateAgentManifest()
{
    Bosch::DeviceAgentManifest agentManifest;

    agentManifest.capabilities.setFlag(
        nx::vms::api::analytics::DeviceAgentManifest::disableStreamSelection);

    static const QString kBoschPrefix("nx.bosch.");
    for (const QString& topic: m_wsntTopics)
        agentManifest.supportedEventTypeIds << kBoschPrefix + ExtractEventTypeNameFromTopic(topic);

    agentManifest.supportedObjectTypeIds << "nx.bosch.ObjectDetection.AnyObject";

    m_handler->pushManifest(new nx::sdk::String(QJson::serialized(agentManifest)));
}

/*virtual*/ void DeviceAgent::doPushDataPacket(
    Result<void>* /*outResult*/, IDataPacket* dataPacket) /*override*/
{
    const auto incomingPacket = dataPacket->queryInterface<ICustomMetadataPacket>();
    QByteArray xmlData(incomingPacket->data(), incomingPacket->dataSize());
    MetadataXmlParser parser(/*test::*/xmlData);
    const ParsedMetadata parsedMetadata = parser.parse();
    if (replanishSupportedEventTypeIds(parsedMetadata))
        updateAgentManifest();

    Ptr<EventMetadataPacket> packet = buildEventPacket(parsedMetadata, dataPacket->timestampUs());

    if (packet && NX_ASSERT(m_handler))
        m_handler->handleMetadata(packet.get());

    Ptr<ObjectMetadataPacket> packet2 = buildObjectPacket(parsedMetadata, dataPacket->timestampUs());
    if (packet2 && NX_ASSERT(m_handler))
        m_handler->handleMetadata(packet.get());

    thread_local int i = 0;
    std::cout << std::endl
        << "====================================================================================="
        << ++i << std::endl
        << /*test::*/xmlData.constData() << std::endl;

}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::bosch
