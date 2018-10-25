#include "resource_analytics_context.h"

#include <nx/utils/log/log.h>
#include <nx/sdk/analytics/consuming_device_agent.h>
#include <nx/sdk/analytics/consuming_device_agent.h>

#include "video_data_receptor.h"
#include "data_packet_adapter.h"

static const int kMaxQueueSize = 3;

namespace nx {
namespace mediaserver {
namespace analytics {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgentContext::DeviceAgentContext(
    int queueSize,
    MetadataHandlerPtr metadataHandler,
    DeviceAgentPtr deviceAgent,
    const nx::vms::api::analytics::EngineManifest& engineManifest):
    base_type(queueSize),
    m_metadataHandler(std::move(metadataHandler)),
    m_deviceAgent(std::move(deviceAgent)),
    m_engineManifest(engineManifest)
{
    m_deviceAgent->setMetadataHandler(m_metadataHandler.get());
}

DeviceAgentContext::~DeviceAgentContext()
{
    stop();
    if (m_deviceAgent)
        m_deviceAgent->stopFetchingMetadata();
}

bool DeviceAgentContext::isStreamConsumer() const
{
    return dynamic_cast<const nx::sdk::analytics::ConsumingDeviceAgent*>(m_deviceAgent.get());
}

void DeviceAgentContext::putData(const QnAbstractDataPacketPtr& data)
{
    if (!isRunning())
        start();
    base_type::putData(data);
}

bool DeviceAgentContext::processData(const QnAbstractDataPacketPtr& data)
{
    // Returning true means the data has been processed.

    auto consumingDeviceAgent = nxpt::ScopedRef<ConsumingDeviceAgent>(
        m_deviceAgent->queryInterface(IID_ConsumingDeviceAgent));
    NX_ASSERT(consumingDeviceAgent);
    if (!consumingDeviceAgent)
        return true;
    auto packetAdapter = std::dynamic_pointer_cast<DataPacketAdapter>(data);
    NX_ASSERT(packetAdapter);
    if (!packetAdapter)
        return true;
    const Error error = consumingDeviceAgent->pushDataPacket(packetAdapter->packet());
    if (error != Error::noError)
    {
        NX_VERBOSE(this, "Plugin %1 has rejected video data with error %2",
            m_engineManifest.pluginName.value, error);
    }
    return true;
}

ResourceAnalyticsContext::ResourceAnalyticsContext()
{
}

ResourceAnalyticsContext::~ResourceAnalyticsContext()
{
    m_videoDataReceptor.clear();
}

bool ResourceAnalyticsContext::canAcceptData() const
{
    return m_metadataReceptor.toStrongRef() != nullptr;
}

void ResourceAnalyticsContext::putData(const QnAbstractDataPacketPtr& data)
{
    if (auto receptor = m_metadataReceptor.toStrongRef())
        receptor->putData(data);
}

void ResourceAnalyticsContext::clearDeviceAgentContexts()
{
    for (auto& deviceEngineContext: m_deviceAgentContexts)
    {
        if (deviceEngineContext->deviceAgent()->stopFetchingMetadata() != Error::noError)
        {
            NX_WARNING(this, lm("Failed to stop fetching metadata from plugin %1")
                .arg(deviceEngineContext->engineManifest().pluginName.value));
        }
    }
    m_deviceAgentContexts.clear();
}

void ResourceAnalyticsContext::addDeviceAgent(
    DeviceAgentPtr deviceAgent,
    MetadataHandlerPtr handler,
    const nx::vms::api::analytics::EngineManifest& manifest)
{

    auto context = std::make_unique<DeviceAgentContext>(
        kMaxQueueSize, std::move(handler), std::move(deviceAgent), manifest);
    m_deviceAgentContexts.push_back(std::move(context));
}

void ResourceAnalyticsContext::setVideoDataReceptor(
    const QSharedPointer<VideoDataReceptor>& receptor)
{
    m_videoDataReceptor = receptor;
}

void ResourceAnalyticsContext::setMetadataDataReceptor(
    QWeakPointer<QnAbstractDataReceptor> receptor)
{
    m_metadataReceptor = receptor;
}

DeviceAgentContextList& ResourceAnalyticsContext::deviceAgentContexts()
{
    return m_deviceAgentContexts;
}

QSharedPointer<VideoDataReceptor> ResourceAnalyticsContext::videoDataReceptor() const
{
    return m_videoDataReceptor;
}

QWeakPointer<QnAbstractDataReceptor> ResourceAnalyticsContext::metadataDataReceptor() const
{
    return m_metadataReceptor;
}

void ResourceAnalyticsContext::setDeviceAgentContextsInitialized(bool value)
{
    m_deviceAgentContextsInitialized = value;
}

bool ResourceAnalyticsContext::areDeviceAgentContextsInitialized() const
{
    return m_deviceAgentContextsInitialized;
}

} // namespace analytics
} // namespace mediaserver
} // namespace nx
