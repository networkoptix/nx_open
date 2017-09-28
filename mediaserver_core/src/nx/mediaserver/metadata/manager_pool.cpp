#include "manager_pool.h"

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


namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

ResourceMetadataContext::ResourceMetadataContext(
    AbstractMetadataManager* metadataManager,
    AbstractMetadataHandler* metadataHandler)
:
    manager(
        metadataManager,
        ManagerDeleter(
            [](AbstractMetadataManager* metadataManager)
            {
                metadataManager->stopFetchingMetadata();
                metadataManager->releaseRef();
            })),
    handler(metadataHandler)
{
}

ManagerPool::ManagerPool(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

void ManagerPool::init()
{
    auto resourcePool = commonModule()->resourcePool();

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
    auto resourcePool = commonModule()->resourcePool();
    const auto mediaServer = resourcePool
        ->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());

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

    releaseResourceMetadataManagers(camera);
}

void ManagerPool::at_rulesUpdated(const std::set<QnUuid>& affectedResources)
{
    for (const auto& resourceId: affectedResources)
    {
        auto resource = commonModule()
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

void ManagerPool::createMetadataManagersForResource(const QnSecurityCamResourcePtr& camera)
{
    NX_ASSERT(camera);
    if (!camera)
        return;

    auto cameraId = camera->getId();
    if (!canFetchMetadataFromResource(camera))
        return;

    releaseResourceMetadataManagers(camera);

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

        m_contexts.emplace(
            camera->getId(),
            ResourceMetadataContext(manager, handler));

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

void ManagerPool::releaseResourceMetadataManagers(const QnSecurityCamResourcePtr& resource)
{
    auto id = resource->getId();
    auto range  = m_contexts.erase(id);
}

AbstractMetadataHandler* ManagerPool::createMetadataHandler(
    const QnResourcePtr& resource,
    const QnUuid& pluginId)
{
    auto handler = new EventHandler();
    handler->setResource(resource);
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
        auto context = m_contexts.find(resourceId);
        if (context == m_contexts.cend())
            createMetadataManagersForResource(camera);
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

bool ManagerPool::fetchMetadataForResource(const QnUuid& resourceId, std::set<QnUuid>& eventTypeIds)
{
    auto context = m_contexts.find(resourceId);
    if (context == m_contexts.cend())
        return false;

    auto& manager = context->second.manager;
    auto& handler = context->second.handler;
    Error result = Error::unknownError;

    if (eventTypeIds.empty())
        result = manager->stopFetchingMetadata();
    else
        result = manager->startFetchingMetadata(handler.get()); //< TODO: #dmishin pass event types.

    return result == Error::noError;
}

boost::optional<nx::api::AnalyticsDriverManifest> ManagerPool::addManifestToServer(
    AbstractMetadataPlugin* plugin)
{
    auto server = commonModule()
        ->resourcePool()
        ->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());

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

    return true;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
