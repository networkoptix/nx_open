#include "resource_metadata_context.h"

#include <nx/utils/log/log.h>
#include <nx/sdk/metadata/consuming_camera_manager.h>

#include "video_data_receptor.h"

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

ManagerContext::~ManagerContext()
{
    if (manager)
        manager->stopFetchingMetadata();
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
        if (managerContext.manager->stopFetchingMetadata() != Error::noError)
        {
            NX_WARNING(this, lm("Failed to stop fetching metadata from plugin %1")
                .arg(managerContext.manifest.pluginName.value));
        }
    }
    m_managers.clear();
}

void ResourceMetadataContext::addManager(
    ManagerPtr manager,
    HandlerPtr handler,
    const nx::api::AnalyticsDriverManifest& manifest)
{

    ManagerContext context;
    nxpt::ScopedRef<CameraManager> consumingManager(
        manager->queryInterface(nx::sdk::metadata::IID_ConsumingCameraManager));

    context.isStreamConsumer = !!consumingManager;
    context.handler = std::move(handler);
    context.manager = std::move(manager);
    context.manager->setHandler(context.handler.get());
    context.manifest = manifest;
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
