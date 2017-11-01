#include "resource_metadata_context.h"
#include "media_data_receptor.h"

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

void ManagerDeletor(nx::sdk::metadata::AbstractMetadataManager* manager)
{
    manager->stopFetchingMetadata();
    manager->releaseRef();
}

ResourceMetadataContext::ResourceMetadataContext()
{
}

ResourceMetadataContext::~ResourceMetadataContext()
{
    if (m_videoFrameDataReceptor)
        m_videoFrameDataReceptor->detachFromContext();
    m_videoFrameDataReceptor.clear();
}

bool ResourceMetadataContext::canAcceptData() const
{
    QnMutexLocker lock(&m_mutex);
    return m_metadataReceptor != nullptr;
}

void ResourceMetadataContext::putData(const QnAbstractDataPacketPtr& data)
{
    QnMutexLocker lock(&m_mutex);
    if (m_metadataReceptor)
        m_metadataReceptor->putData(data);
}

void ResourceMetadataContext::clearManagers()
{
    QnMutexLocker lock(&m_mutex);
    m_managers.clear();
}

void ResourceMetadataContext::addManager(
    nx::sdk::metadata::AbstractMetadataManager* manager,
    nx::sdk::metadata::AbstractMetadataHandler* handler,
    const nx::api::AnalyticsDriverManifest& manifest)
{
    QnMutexLocker lock(&m_mutex);
    ManagerContext context;
    context.manager = ManagerPtr(manager, ManagerDeletor);
    context.handler.reset(handler);
    context.manifest = manifest;
    m_managers.push_back(std::move(context));
}

void ResourceMetadataContext::setDataProvider(const QnAbstractMediaStreamDataProvider* provider)
{
    QnMutexLocker lock(&m_mutex);
    m_dataProvider = provider;
}

void ResourceMetadataContext::setVideoFrameDataReceptor(const QSharedPointer<VideoDataReceptor>& receptor)
{
    QnMutexLocker lock(&m_mutex);
    m_videoFrameDataReceptor = receptor;
}

void ResourceMetadataContext::setMetadataDataReceptor(QnAbstractDataReceptor* receptor)
{
    QnMutexLocker lock(&m_mutex);
    m_metadataReceptor = receptor;
}

const ManagerList& ResourceMetadataContext::managers() const
{
    QnMutexLocker lock(&m_mutex);
    return m_managers;
}

const QnAbstractMediaStreamDataProvider* ResourceMetadataContext::dataProvider() const
{
    QnMutexLocker lock(&m_mutex);
    return m_dataProvider;
}

QSharedPointer<VideoDataReceptor> ResourceMetadataContext::videoFrameDataReceptor() const
{
    QnMutexLocker lock(&m_mutex);
    return m_videoFrameDataReceptor;
}

QnAbstractDataReceptor* ResourceMetadataContext::metadataDataReceptor() const
{
    QnMutexLocker lock(&m_mutex);
    return m_metadataReceptor;
}

void ResourceMetadataContext::setManagersInitialized(bool value)
{
    QnMutexLocker lock(&m_mutex);
    m_isManagerInitialized = value;
}

bool ResourceMetadataContext::isManagerInitialized() const
{
    QnMutexLocker lock(&m_mutex);
    return m_isManagerInitialized;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
