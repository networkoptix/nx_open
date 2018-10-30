#include "device_agent.h"

#include "common.h"

#include <chrono>

#include <nx/sdk/analytics/common_event.h>
#include <nx/sdk/analytics/common_metadata_packet.h>
#include <nx/mediaserver_plugins/utils/uuid.h>
#include <nx/utils/log/log_main.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace hikvision {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine):
    m_engine(engine)
{
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
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

void DeviceAgent::setSettings(const nx::sdk::Settings* settings)
{
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::Settings* DeviceAgent::settings() const
{
    return nullptr;
}

nx::sdk::Error DeviceAgent::setMetadataHandler(
    nx::sdk::analytics::MetadataHandler* metadataHandler)
{
    m_metadataHandler = metadataHandler;
    return nx::sdk::Error::noError;
}

nx::sdk::Error DeviceAgent::setNeededMetadataTypes(
    const nx::sdk::analytics::IMetadataTypes* metadataTypes)
{
    if (metadataTypes->isNull())
    {
        stopFetchingMetadata();
        return Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

nx::sdk::Error DeviceAgent::startFetchingMetadata(
    const nx::sdk::analytics::IMetadataTypes* metadataTypes)
{
    auto monitorHandler =
        [this](const HikvisionEventList& events)
        {
            using namespace std::chrono;
            auto packet = new CommonEventsMetadataPacket();

            for (const auto& hikvisionEvent: events)
            {
                if (hikvisionEvent.channel.is_initialized() && hikvisionEvent.channel != m_channel)
                    return;

                auto event = new CommonEvent();
                NX_VERBOSE(this, lm("Got event: %1 %2 Channel %3")
                    .arg(hikvisionEvent.caption).arg(hikvisionEvent.description).arg(m_channel));

                event->setTypeId(hikvisionEvent.typeId.toStdString());
                event->setCaption(hikvisionEvent.caption.toStdString());
                event->setDescription(hikvisionEvent.description.toStdString());
                event->setIsActive(hikvisionEvent.isActive);
                event->setConfidence(1.0);
                //event->setAuxilaryData(hikvisionEvent.fullEventName.toStdString());

                packet->setTimestampUsec(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());

                packet->setDurationUsec(-1);
                packet->addItem(event);
            }

            m_metadataHandler->handleMetadata(Error::noError, packet);
        };

    NX_ASSERT(m_engine);
    std::vector<QString> eventTypes;

    const auto eventTypeList = metadataTypes->eventTypeIds();
    for (int i = 0; i < eventTypeList->count(); ++i)
        eventTypes.push_back(eventTypeList->at(i));

    m_monitor =
        std::make_unique<HikvisionMetadataMonitor>(
            m_engine->engineManifest(),
            QJson::deserialized<nx::vms::api::analytics::DeviceAgentManifest>(
                m_deviceAgentManifest),
            m_url,
            m_auth,
            eventTypes);

    m_monitor->addHandler(m_uniqueId, monitorHandler);
    m_monitor->startMonitoring();

    return Error::noError;
}

void DeviceAgent::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_engine);

    m_monitor = nullptr;
}

const char* DeviceAgent::manifest(Error* error)
{
    if (m_deviceAgentManifest.isEmpty())
    {
        *error = Error::unknownError;
        return nullptr;
    }

    *error = Error::noError;
    return m_deviceAgentManifest.constData();
}

void DeviceAgent::freeManifest(const char* data)
{
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

void DeviceAgent::setEngineManifest(const Hikvision::EngineManifest& manifest)
{
    m_engineManifest = manifest;
}

} // namespace hikvision
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
