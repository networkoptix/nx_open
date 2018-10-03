#include "resource_metadata_context.h"

#include <nx/utils/log/log.h>
#include <nx/sdk/metadata/consuming_camera_manager.h>
#include <nx/sdk/metadata/consuming_camera_manager.h>

#include "video_data_receptor.h"
#include "data_packet_adapter.h"

static const int kMaxQueueSize = 3;

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

ManagerContext::ManagerContext(
    int queueSize,
    HandlerPtr handler,
    ManagerPtr manager,
    const nx::vms::api::analytics::PluginManifest& manifest):
    base_type(queueSize),
    m_handler(std::move(handler)),
    m_manager(std::move(manager)),
    m_manifest(manifest)
{
    m_manager->setHandler(m_handler.get());
}

ManagerContext::~ManagerContext()
{
    stop();
    if (m_manager)
        m_manager->stopFetchingMetadata();
}

bool ManagerContext::isStreamConsumer() const
{
    return dynamic_cast<const nx::sdk::metadata::ConsumingCameraManager*>(m_manager.get());
}

void ManagerContext::putData(const QnAbstractDataPacketPtr& data)
{
    if (!isRunning())
        start();
    base_type::putData(data);
}

bool ManagerContext::processData(const QnAbstractDataPacketPtr& data)
{
    // Returning true means the data has been processed.

    auto consumingManager = nxpt::ScopedRef<ConsumingCameraManager>(
        m_manager->queryInterface(IID_ConsumingCameraManager));
    NX_ASSERT(consumingManager);
    if (!consumingManager)
        return true;
    auto packetAdapter = std::dynamic_pointer_cast<DataPacketAdapter>(data);
    NX_ASSERT(packetAdapter);
    if (!packetAdapter)
        return true;
    const Error error = consumingManager->pushDataPacket(packetAdapter->packet());
    if (error != Error::noError)
    {
        NX_VERBOSE(this, "Plugin %1 has rejected video data with error %2",
            m_manifest.pluginName.value, error);
    }
    return true;
}

ResourceMetadataContext::ResourceMetadataContext()
{
}

ResourceMetadataContext::~ResourceMetadataContext()
{
    m_videoDataReceptor.clear();
}

bool ResourceMetadataContext::canAcceptData() const
{
    return m_metadataReceptor.toStrongRef() != nullptr;
}

void ResourceMetadataContext::putData(const QnAbstractDataPacketPtr& data)
{
    if (auto receptor = m_metadataReceptor.toStrongRef())
        receptor->putData(data);
}

void ResourceMetadataContext::clearManagers()
{
    for (auto& managerContext: m_managers)
    {
        if (managerContext->manager()->stopFetchingMetadata() != Error::noError)
        {
            NX_WARNING(this, lm("Failed to stop fetching metadata from plugin %1")
                .arg(managerContext->manifest().pluginName.value));
        }
    }
    m_managers.clear();
}

void ResourceMetadataContext::addManager(
    ManagerPtr manager,
    HandlerPtr handler,
    const nx::vms::api::analytics::PluginManifest& manifest)
{

    auto context = std::make_unique<ManagerContext>(
        kMaxQueueSize, std::move(handler), std::move(manager), manifest);
    m_managers.push_back(std::move(context));
}

void ResourceMetadataContext::setVideoDataReceptor(
    const QSharedPointer<VideoDataReceptor>& receptor)
{
    m_videoDataReceptor = receptor;
}

void ResourceMetadataContext::setMetadataDataReceptor(
    QWeakPointer<QnAbstractDataReceptor> receptor)
{
    m_metadataReceptor = receptor;
}

ManagerList& ResourceMetadataContext::managers()
{
    return m_managers;
}

QSharedPointer<VideoDataReceptor> ResourceMetadataContext::videoDataReceptor() const
{
    return m_videoDataReceptor;
}

QWeakPointer<QnAbstractDataReceptor> ResourceMetadataContext::metadataDataReceptor() const
{
    return m_metadataReceptor;
}

void ResourceMetadataContext::setManagersInitialized(bool value)
{
    m_isManagerInitialized = value;
}

bool ResourceMetadataContext::isManagerInitialized() const
{
    return m_isManagerInitialized;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
