#include "resource_metadata_context.h"
#include "media_data_receptor.h"

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

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
    ManagerPtr manager,
    HandlerPtr handler,
    const nx::api::AnalyticsDriverManifest& manifest)
{
    QnMutexLocker lock(&m_mutex);
    ManagerContext context;
    context.handler = std::move(handler);
    context.manager = std::move(manager);
    context.manager->setHandler(context.handler.get());
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

ManagerList& ResourceMetadataContext::managers()
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
