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

void DeviceAgent::setDeclaredSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::Error DeviceAgent::setMetadataHandler(
    nx::sdk::analytics::MetadataHandler* metadataHandler)
{
    m_metadataHandler = metadataHandler;
    return nx::sdk::Error::noError;
}

Error DeviceAgent::startFetchingMetadata(const char* const* typeList, int typeListSize)
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
    for (int i = 0; i < typeListSize; ++i)
        eventTypes.push_back(typeList[i]);
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

Error DeviceAgent::stopFetchingMetadata()
{
    if (m_monitor)
        m_monitor->removeHandler(m_uniqueId);

    NX_ASSERT(m_engine);

    m_monitor = nullptr;
    return Error::noError;
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
