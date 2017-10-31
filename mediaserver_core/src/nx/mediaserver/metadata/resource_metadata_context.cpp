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
    if (m_manager)
    {
        m_manager->stopFetchingMetadata();
        m_manager->releaseRef();
    }
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

void ResourceMetadataContext::setManager(nx::sdk::metadata::AbstractMetadataManager* manager)
{
    QnMutexLocker lock(&m_mutex);
    m_manager.reset(manager);
}

void ResourceMetadataContext::setHandler(nx::sdk::metadata::AbstractMetadataHandler* handler)
{
    QnMutexLocker lock(&m_mutex);
    m_handler.reset(handler);
}

void ResourceMetadataContext::setPluginManifest(const nx::api::AnalyticsDriverManifest& manifest)
{
    m_pluginManifest = manifest;
}

nx::api::AnalyticsDriverManifest ResourceMetadataContext::pluginManifest() const
{
    return m_pluginManifest;
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

nx::sdk::metadata::AbstractMetadataManager* ResourceMetadataContext::manager() const
{
    QnMutexLocker lock(&m_mutex);
    return m_manager.get();
}

nx::sdk::metadata::AbstractMetadataHandler* ResourceMetadataContext::handler() const
{
    QnMutexLocker lock(&m_mutex);
    return m_handler.get();
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

} // namespace metadata
} // namespace mediaserver
} // namespace nx
