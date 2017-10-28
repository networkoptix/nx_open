#include "manager_pool.h"

#include <common/common_module.h>
#include <media_server/media_server_module.h>

#include <plugins/plugin_manager.h>
#include <plugins/plugin_tools.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/fusion/serialization/json.h>

#include <nx/mediaserver/metadata/event_handler.h>
#include <nx/mediaserver/metadata/event_rule_watcher.h>

#include <nx/api/analytics/supported_events.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/sdk/metadata/abstract_consuming_metadata_manager.h>
#include <core/dataconsumer/abstract_data_receptor.h>
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

ManagerPool::ManagerPool(QnMediaServerModule* serverModule):
    m_serverModule(serverModule)
{
}

ManagerPool::~ManagerPool()
{
    disconnect(this);
}

void ManagerPool::init()
{
    auto resourcePool = m_serverModule->commonModule()->resourcePool();

    connect(
        resourcePool, &QnResourcePool::resourceAdded,
        this, &ManagerPool::at_resourceAdded);

    connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        this, &ManagerPool::at_resourceRemoved);

    connect(
        qnServerModule->metadataRuleWatcher(), &EventRuleWatcher::rulesUpdated,
        this, &ManagerPool::at_rulesUpdated);

    QMetaObject::invokeMethod(this, "initExistingResources");
}

void ManagerPool::initExistingResources()
{
    auto resourcePool = m_serverModule->commonModule()->resourcePool();
    const auto mediaServer = resourcePool
        ->getResourceById<QnMediaServerResource>(
            m_serverModule->commonModule()->moduleGUID());

    const auto cameras = resourcePool->getAllCameras(
        mediaServer,
        /*ignoreDesktopCamera*/ true);

    for (const auto& camera: cameras)
        at_resourceAdded(camera);
}

void ManagerPool::at_resourceAdded(const QnResourcePtr& resource)
{
    connect(
        resource.data(), &QnResource::statusChanged,
        this, &ManagerPool::handleResourceChanges);

    connect(
        resource.data(), &QnResource::parentIdChanged,
        this, &ManagerPool::handleResourceChanges);

    handleResourceChanges(resource);
}

void ManagerPool::at_resourceRemoved(const QnResourcePtr& resource)
{
    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    QnMutexLocker lock(&m_contextMutex);
    releaseResourceMetadataManagersUnsafe(camera);
}

void ManagerPool::at_rulesUpdated(const QSet<QnUuid>& affectedResources)
{
    for (const auto& resourceId: affectedResources)
    {
        auto resource = m_serverModule
            ->commonModule()
            ->resourcePool()
            ->getResourceById(resourceId);

        handleResourceChanges(resource);
    }
}

nx::mediaserver::metadata::ManagerPool::PluginList ManagerPool::availablePlugins() const
{
    auto pluginManager = qnServerModule->pluginManager();
    NX_ASSERT(pluginManager, lit("Can not access PluginManager instance"));
    if (!pluginManager)
        return PluginList();

    return pluginManager->findNxPlugins<AbstractMetadataPlugin>(
        IID_MetadataPlugin);
}

void ManagerPool::createMetadataManagersForResourceUnsafe(const QnSecurityCamResourcePtr& camera)
{
    NX_ASSERT(camera);
    if (!camera)
        return;

    auto cameraId = camera->getId();
    if (!canFetchMetadataFromResource(camera))
        return;

    releaseResourceMetadataManagersUnsafe(camera);

    auto plugins = availablePlugins();

    for (auto& plugin: plugins)
    {
        auto manager = createMetadataManager(camera, plugin);
        if (!manager)
            continue;

        nxpt::ScopedRef<AbstractMetadataManager> managerGuard(manager, false);

        auto pluginManifest = addManifestToServer(plugin);
        if (!pluginManifest)
            continue;

        auto deviceManifest = addManifestToCamera(camera, manager);
        if (!deviceManifest)
            continue;

        auto handler = createMetadataHandler(camera, pluginManifest->driverId);
        if (!handler)
            continue;

        auto& context = m_contexts[camera->getId()];
        context.setManager(manager);
        context.setHandler(handler);
        handler->registerDataReceptor(&context);

        managerGuard.release();
    }
}

AbstractMetadataManager* ManagerPool::createMetadataManager(
    const QnSecurityCamResourcePtr& camera,
    AbstractMetadataPlugin* plugin) const
{
    Error error = Error::noError;
    ResourceInfo resourceInfo;
    bool success = resourceInfoFromResource(camera, &resourceInfo);
    if (!success)
        return nullptr;

    return plugin->managerForResource(resourceInfo, &error);
}

void ManagerPool::releaseResourceMetadataManagersUnsafe(const QnSecurityCamResourcePtr& resource)
{
    m_contexts.erase(resource->getId());
}

EventHandler* ManagerPool::createMetadataHandler(
    const QnResourcePtr& resource,
    const QnUuid& pluginId)
{
    auto handler = new EventHandler();
    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return nullptr;

    handler->setResource(camera);
    handler->setPluginId(pluginId);

    return handler;
}

void ManagerPool::handleResourceChanges(const QnResourcePtr& resource)
{
    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    auto resourceId = camera->getId();
    auto events = qnServerModule
        ->metadataRuleWatcher()
        ->watchedEventsForResource(resourceId);

    if (canFetchMetadataFromResource(camera))
    {
        QnMutexLocker lock(&m_contextMutex);
        auto context = m_contexts.find(resourceId);
        if (context == m_contexts.cend())
            createMetadataManagersForResourceUnsafe(camera);
    }

    fetchMetadataForResource(resourceId, events);
}

bool ManagerPool::canFetchMetadataFromResource(const QnSecurityCamResourcePtr& camera) const
{
    if (!camera)
        return false;

    const auto status = camera->getStatus();
    return !camera->hasFlags(Qn::foreigner | Qn::desktop_camera)
        && (status == Qn::Online || status == Qn::Recording);
}

bool ManagerPool::fetchMetadataForResource(const QnUuid& resourceId, QSet<QnUuid>& eventTypeIds)
{
    QnMutexLocker lock(&m_contextMutex);
    auto context = m_contexts.find(resourceId);
    if (context == m_contexts.cend())
        return false;

    auto manager = context->second.manager();
    auto handler = context->second.handler();
    Error result = Error::unknownError;
    if (!manager)
        return false;

    if (eventTypeIds.empty())
        result = manager->stopFetchingMetadata();
    else
        result = manager->startFetchingMetadata(handler); //< TODO: #dmishin pass event types.

    return result == Error::noError;
}

boost::optional<nx::api::AnalyticsDriverManifest> ManagerPool::addManifestToServer(
    AbstractMetadataPlugin* plugin)
{
    auto server = m_serverModule
        ->commonModule()
        ->resourcePool()
        ->getResourceById<QnMediaServerResource>(
            m_serverModule->commonModule()->moduleGUID());

    if (!server)
        return boost::none;

    Error error = Error::noError;
    auto pluginManifest = deserializeManifest<nx::api::AnalyticsDriverManifest>(
        plugin->capabilitiesManifest(&error));

    if (!pluginManifest || error != Error::noError)
        boost::none;

    bool overwritten = false;
    auto existingManifests = server->analyticsDrivers();
    for (auto& existingManifest : existingManifests)
    {
        if (existingManifest.driverId == pluginManifest->driverId)
        {
            existingManifest = *pluginManifest;
            overwritten = true;
            break;
        }
    }

    if (!overwritten)
        existingManifests.push_back(*pluginManifest);

    server->setAnalyticsDrivers(existingManifests);
    server->saveParams();

    return pluginManifest;
}

boost::optional<nx::api::AnalyticsDeviceManifest> ManagerPool::addManifestToCamera(
    const QnSecurityCamResourcePtr& camera,
    AbstractMetadataManager* manager)
{
    NX_ASSERT(manager);
    NX_ASSERT(camera);

    Error error = Error::noError;
    auto deviceManifest = deserializeManifest<nx::api::AnalyticsDeviceManifest>(
        manager->capabilitiesManifest(&error));

    if (!deviceManifest || error != Error::noError)
        return boost::none;

    camera->setAnalyticsSupportedEvents(deviceManifest->supportedEventTypes);
    camera->saveParams();

    return deviceManifest;
}

bool ManagerPool::resourceInfoFromResource(
    const QnSecurityCamResourcePtr& camera,
    ResourceInfo* outResourceInfo) const
{
    NX_ASSERT(camera);
    NX_ASSERT(outResourceInfo);

    strncpy(
        outResourceInfo->vendor,
        camera->getVendor().toUtf8().data(),
        ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->model,
        camera->getModel().toUtf8().data(),
        ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->firmware,
        camera->getFirmware().toUtf8().data(),
        ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->uid,
        camera->getId().toByteArray().data(),
        ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->sharedId,
        camera->getSharedId().toStdString().c_str(),
        ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->url,
        camera->getUrl().toUtf8().data(),
        ResourceInfo::kTextParameterMaxLength);

    auto auth = camera->getAuth();
    strncpy(
        outResourceInfo->login,
        auth.user().toUtf8().data(),
        ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->password,
        auth.password().toUtf8().data(),
        ResourceInfo::kStringParameterMaxLength);

    outResourceInfo->channel = camera->getChannel();

    return true;
}

QWeakPointer<QnAbstractDataReceptor> ManagerPool::registerDataProvider(QnAbstractMediaStreamDataProvider* dataProvider)
{
    QnMutexLocker lock(&m_contextMutex);
    auto id = dataProvider->getResource()->getId();
    auto& context = m_contexts[id];
    context.setDataProvider(dataProvider);
    auto dataReceptor = VideoDataReceptorPtr(new VideoDataReceptor(&context));
    context.setVideoFrameDataReceptor(dataReceptor);
    return dataReceptor.toWeakRef();
}

void ManagerPool::removeDataProvider(QnAbstractMediaStreamDataProvider* dataProvider)
{
    QnMutexLocker lock(&m_contextMutex);
    const auto& id = dataProvider->getResource()->getId();
    auto itr = m_contexts.find(id);
    if (itr == m_contexts.end())
        return;
    auto& context = itr->second;
    context.setDataProvider(nullptr);
    context.setVideoFrameDataReceptor(VideoDataReceptorPtr());
}

void ManagerPool::registerDataReceptor(
    const QnResourcePtr& resource,
    QnAbstractDataReceptor* dataReceptor)
{
    QnMutexLocker lock(&m_contextMutex);
    const auto& id = resource->getId();

    auto& context = m_contexts[id];
    context.setMetadataDataReceptor(dataReceptor);
}

void ManagerPool::removeDataReceptor(
    const QnResourcePtr& resource,
    QnAbstractDataReceptor* dataReceptor)
{
    QnMutexLocker lock(&m_contextMutex);
    auto itr = m_contexts.find(resource->getId());
    if (itr == m_contexts.end())
        return;
    auto& context = itr->second;
    context.setMetadataDataReceptor(nullptr);
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
