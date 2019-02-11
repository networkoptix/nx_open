#include "device_agent.h"

#include <chrono>

#include <QtCore/QUrl>

#define NX_PRINT_PREFIX "[hanwha::DeviceAgent] "
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/utils/log/log.h>

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

Error DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    m_handler = handler;
    return Error::noError;
}

Error DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes)
{
    if (metadataTypes->eventTypeIds()->count() == 0)
    {
        stopFetchingMetadata();
        return Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

void DeviceAgent::setSettings(const IStringMap* settings)
{
    // There are no DeviceAgent settings for this plugin.
}

IStringMap* DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

Error DeviceAgent::startFetchingMetadata(
    const IMetadataTypes* /*metadataTypes*/)
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

                auto eventMetadata = new nx::sdk::analytics::EventMetadata();
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
                eventMetadata->setAuxiliaryData(hanwhaEvent.fullEventName.toStdString());

                eventMetadataPacket->setTimestampUs(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                eventMetadataPacket->setDurationUs(-1);
                eventMetadataPacket->addItem(eventMetadata);
            }

            NX_ASSERT(m_handler, "Handler should exist");
            if (m_handler)
                m_handler->handleMetadata(eventMetadataPacket);

            eventMetadataPacket->releaseRef();
        };

    NX_ASSERT(m_engine);
    m_monitor = m_engine->monitor(m_sharedId, m_url, m_auth);
    if (!m_monitor)
        return Error::unknownError;

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();

    return Error::noError;
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

const IString* DeviceAgent::manifest(Error* error) const
{
    if (m_deviceAgentManifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
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
