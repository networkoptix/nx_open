#include "device_agent.h"

#include <chrono>

#include <QtCore/QUrl>

#define NX_PRINT_PREFIX "[hanwha::DeviceAgent] "
#include <nx/kit/debug.h>

#include <nx/sdk/common/string.h>

#include <nx/sdk/analytics/common_event.h>
#include <nx/sdk/analytics/common_metadata_packet.h>
#include <nx/utils/log/log.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
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

void* DeviceAgent::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_DeviceAgent)
    {
        addRef();
        return static_cast<DeviceAgent*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

nx::sdk::Error DeviceAgent::setHandler(nx::sdk::analytics::DeviceAgent::IHandler* handler)
{
    m_handler = handler;
    return Error::noError;
}

nx::sdk::Error DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes)
{
    if (metadataTypes->eventTypeIds()->count() == 0)
    {
        stopFetchingMetadata();
        return Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

void DeviceAgent::setSettings(const nx::sdk::Settings* settings)
{
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::Settings* DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

nx::sdk::Error DeviceAgent::startFetchingMetadata(
    const nx::sdk::analytics::IMetadataTypes* /*metadataTypes*/)
{
    const auto monitorHandler =
        [this](const EventList& events)
        {
            using namespace std::chrono;
            auto packet = new CommonEventsMetadataPacket();

            for (const auto& hanwhaEvent: events)
            {
                if (hanwhaEvent.channel.is_initialized() && hanwhaEvent.channel != m_channel)
                    return;

                auto event = new CommonEvent();
                NX_PRINT
                    << "Got event: caption ["
                    << hanwhaEvent.caption.toStdString() << "], description ["
                    << hanwhaEvent.description.toStdString() << "], "
                    << "channel " << m_channel;

                NX_VERBOSE(this, lm("Got event: %1 %2 on channel %3").args(
                    hanwhaEvent.caption, hanwhaEvent.description, m_channel));

                event->setTypeId(hanwhaEvent.typeId.toStdString());
                event->setCaption(hanwhaEvent.caption.toStdString());
                event->setDescription(hanwhaEvent.caption.toStdString());
                event->setIsActive(hanwhaEvent.isActive);
                event->setConfidence(1.0);
                event->setAuxilaryData(hanwhaEvent.fullEventName.toStdString());

                packet->setTimestampUsec(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                packet->setDurationUsec(-1);
                packet->addItem(event);
            }

            NX_ASSERT(m_handler, "Handler should exist");
            if (m_handler)
                m_handler->handleMetadata(packet);

            packet->releaseRef();
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
    return new common::String(m_deviceAgentManifest);
}

void DeviceAgent::setDeviceInfo(const nx::sdk::DeviceInfo& deviceInfo)
{
    m_url = deviceInfo.url;
    m_model = deviceInfo.model;
    m_firmware = deviceInfo.firmware;
    m_auth.setUser(deviceInfo.login);
    m_auth.setPassword(deviceInfo.password);
    m_uniqueId = deviceInfo.uid;
    m_sharedId = deviceInfo.sharedId;
    m_channel = deviceInfo.channel;
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
} // namespace mediaserver_plugins
} // namespace nx
