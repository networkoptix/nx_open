#include "resource_metadata_context.h"
#include "media_data_receptor.h"

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
    m_videoFrameDataReceptor.clear();
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
    m_managers.clear();
}

void ResourceMetadataContext::addManager(
    ManagerPtr manager,
    HandlerPtr handler,
    const nx::api::AnalyticsDriverManifest& manifest)
{
    ManagerContext context;
    context.handler = std::move(handler);
    context.manager = std::move(manager);
    context.manager->setHandler(context.handler.get());
    context.manifest = manifest;
    m_managers.push_back(std::move(context));
}

void ResourceMetadataContext::setVideoFrameDataReceptor(const QSharedPointer<VideoDataReceptor>& receptor)
{
    m_videoFrameDataReceptor = receptor;
}

void ResourceMetadataContext::setMetadataDataReceptor(QWeakPointer<QnAbstractDataReceptor> receptor)
{
    m_metadataReceptor = receptor;
}

ManagerList& ResourceMetadataContext::managers()
{
    return m_managers;
}

QSharedPointer<VideoDataReceptor> ResourceMetadataContext::videoFrameDataReceptor() const
{
    return m_videoFrameDataReceptor;
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
