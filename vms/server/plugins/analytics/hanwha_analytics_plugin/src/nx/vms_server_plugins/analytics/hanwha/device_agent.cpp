#include "device_agent.h"

#include <chrono>

#include <QtCore/QUrl>

#define NX_PRINT_PREFIX "[hanwha::DeviceAgent] "
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/utils/log/log.h>
#include <nx/vms/server/analytics/predefined_attributes.h>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine):
    m_engine(engine)
{
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    m_engine->deviceAgentIsAboutToBeDestroyed(m_sharedId);
}

void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    handler->addRef();
    m_handler.reset(handler);
}

void DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes, IError* outError)
{
    nx::sdk::Ptr<const nx::sdk::IStringList> eventTypeIds(metadataTypes->eventTypeIds());
    if (const char* message = "Event type id list is nullptr"; !NX_ASSERT(eventTypeIds, message))
    {
        outError->setError(ErrorCode::internalError, message);
        return;
    }

    if (eventTypeIds->count() == 0)
        stopFetchingMetadata();

    return startFetchingMetadata(metadataTypes, outError);
}

void DeviceAgent::setSettings(const IStringMap* /*settings*/, IError* /*outError*/)
{
    // There are no DeviceAgent settings for this plugin.
}

IStringMap* DeviceAgent::pluginSideSettings(IError* /*outError*/) const
{
    return nullptr;
}

void DeviceAgent::startFetchingMetadata(const IMetadataTypes* /*metadataTypes*/, IError* outError)
{
    const auto monitorHandler =
        [this](const EventList& events)
        {
            using namespace std::chrono;
            auto eventMetadataPacket = new EventMetadataPacket();

            for (const auto& hanwhaEvent: events)
            {
                if (hanwhaEvent.channel.is_initialized() && hanwhaEvent.channel != m_channelNumber)
                    return;

                auto eventMetadata = makePtr<EventMetadata>();
                NX_PRINT
                    << "Got event: caption ["
                    << hanwhaEvent.caption.toStdString() << "], description ["
                    << hanwhaEvent.description.toStdString() << "], "
                    << "channel " << m_channelNumber;

                NX_VERBOSE(this, lm("Got event: %1 %2 on channel %3").args(
                    hanwhaEvent.caption, hanwhaEvent.description, m_channelNumber));

                eventMetadata->setTypeId(hanwhaEvent.typeId.toStdString());
                eventMetadata->setCaption(hanwhaEvent.caption.toStdString());
                eventMetadata->setDescription(hanwhaEvent.caption.toStdString());
                eventMetadata->setIsActive(hanwhaEvent.isActive);
                eventMetadata->setConfidence(1.0);
                eventMetadata->addAttribute(makePtr<Attribute>(
                    IAttribute::Type::string,
                    nx::vms::server::analytics::kInputPortIdAttribute,
                    hanwhaEvent.fullEventName.toStdString()));

                eventMetadataPacket->setTimestampUs(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                eventMetadataPacket->setDurationUs(-1);
                eventMetadataPacket->addItem(eventMetadata.get());
            }

            if (!NX_ASSERT(!m_handler))
                m_handler->handleMetadata(eventMetadataPacket);

            eventMetadataPacket->releaseRef();
        };

    if (!NX_ASSERT(m_engine))
    {
        outError->setError(ErrorCode::internalError, "Unable to access the Engine");
        return;
    }

    m_monitor = m_engine->monitor(m_sharedId, m_url, m_auth);
    if (!m_monitor)
    {
        outError->setError(ErrorCode::internalError, "Unable to access the monitor");
        return;
    }

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();
}

void DeviceAgent::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_engine);
    if (m_engine)
        m_engine->deviceAgentStoppedToUseMonitor(m_sharedId);

    m_monitor = nullptr;
}

const IString* DeviceAgent::manifest(IError* outError) const
{
    if (m_deviceAgentManifest.isEmpty())
    {
        outError->setError(ErrorCode::internalError, "DeviceAgent manifest is empty");
        return nullptr;
    }

    return new nx::sdk::String(m_deviceAgentManifest);
}

void DeviceAgent::setDeviceInfo(const IDeviceInfo* deviceInfo)
{
    m_url = deviceInfo->url();
    m_model = deviceInfo->model();
    m_firmware = deviceInfo->firmware();
    m_auth.setUser(deviceInfo->login());
    m_auth.setPassword(deviceInfo->password());
    m_uniqueId = deviceInfo->id();
    m_sharedId = deviceInfo->sharedId();
    m_channelNumber = deviceInfo->channelNumber();
}

void DeviceAgent::setDeviceAgentManifest(const QByteArray& manifest)
{
    m_deviceAgentManifest = manifest;
}

void DeviceAgent::setEngineManifest(const Hanwha::EngineManifest& manifest)
{
    m_engineManifest = manifest;
}

void DeviceAgent::setMonitor(MetadataMonitor* monitor)
{
    m_monitor = monitor;
}

} // namespace hanwha
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
