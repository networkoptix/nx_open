#include "manager_pool.h"

#include <memory>

#include <common/common_module.h>
#include <media_server/media_server_module.h>

#include <plugins/plugin_manager.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_internal_tools.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/api/analytics/device_manifest.h>
#include <nx/api/analytics/supported_events.h>
#include <nx/fusion/serialization/json.h>
#include <nx/mediaserver/metadata/event_handler.h>
#include <nx/mediaserver/metadata/event_rule_watcher.h>
#include <nx/utils/log/log.h>

uint qHash(const nx::api::AnalyticsEventType& t)
{
    return qHash(t.eventTypeId.toByteArray());
}

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

ResourceMetadataContext::ResourceMetadataContext(
    AbstractMetadataManager* metadataManager,
    AbstractMetadataHandler* metadataHandler)
:
    handler(metadataHandler),
    manager(
        metadataManager,
        ManagerDeleter(
            [](AbstractMetadataManager* metadataManager)
            {
                metadataManager->stopFetchingMetadata();
                metadataManager->releaseRef();
            }))
{
}

ManagerPool::ManagerPool(QnMediaServerModule* serverModule):
    m_serverModule(serverModule)
{
}

ManagerPool::~ManagerPool()
{
    NX_DEBUG(this, lit("Destroying metadata manager pool."));
    disconnect(this);
}

void ManagerPool::init()
{
    NX_DEBUG(this, lit("Initializing metdata manager pool."));
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
    NX_VERBOSE(
        this,
        lm("Resource %1 (%2) has been added.")
            .args(resource->getName(), resource->getId()));

    connect(
        resource.data(), &QnResource::statusChanged,
        this, &ManagerPool::handleResourceChanges);

    connect(
        resource.data(), &QnResource::parentIdChanged,
        this, &ManagerPool::handleResourceChanges);

    connect(
        resource.data(), &QnResource::urlChanged,
        this, &ManagerPool::handleResourceChanges);

    connect(
        resource.data(), &QnResource::propertyChanged,
        this, &ManagerPool::at_propertyChanged);

    handleResourceChanges(resource);
}

void ManagerPool::at_propertyChanged(const QnResourcePtr& resource, const QString& name)
{
    if (name == Qn::CAMERA_CREDENTIALS_PARAM_NAME)
    {
        NX_DEBUG(
            this,
            lm("Credentials have been changed for resource %1 (%2)")
                .args(resource->getName(), resource->getId()));

        handleResourceChanges(resource);
    }
}

void ManagerPool::at_resourceRemoved(const QnResourcePtr& resource)
{
    NX_VERBOSE(
        this,
        lm("Resource %1 (%2) has been removed.")
            .args(resource->getName(), resource->getId()));

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    releaseResourceMetadataManagers(camera);
}

void ManagerPool::at_rulesUpdated(const QSet<QnUuid>& affectedResources)
{
    NX_VERBOSE(
        this,
        lm("Rules have been updated. Affected resources: %1").arg(affectedResources));

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

void ManagerPool::createMetadataManagersForResource(const QnSecurityCamResourcePtr& camera)
{
    NX_ASSERT(camera);
    if (!camera)
        return;

    auto cameraId = camera->getId();
    if (!canFetchMetadataFromResource(camera))
    {
        NX_DEBUG(
            this,
            lm("Metadata from resource %1 (%2) can not be fetched.")
                .args(camera->getUserDefinedName(), camera->getId()));
        return;
    }

    QnMediaServerResourcePtr server = m_serverModule
        ->commonModule()
        ->resourcePool()
        ->getResourceById<QnMediaServerResource>(
            m_serverModule->commonModule()->moduleGUID());
    NX_ASSERT(server, lm("Can not obtain current server resource."));
    if (!server)
        return;

    releaseResourceMetadataManagers(camera);

    auto plugins = availablePlugins();
    for (auto& plugin: plugins)
    {
        NX_ASSERT(plugin, lm("Plugin pointer is invalid."));
        if (!plugin)
            continue;

        nxpt::ScopedRef<AbstractMetadataPlugin> pluginGuard(plugin, /*releaseRef*/ false);

        NX_DEBUG(
            this,
            lm("Trying to create metadata manager for resource %1 (%2) using plugin %3.")
                .args(camera->getUserDefinedName(), camera->getId(), plugin->name()));

        auto manager = createMetadataManager(camera, plugin);
        if (!manager)
        {
            NX_DEBUG(
                this,
                lm("Plugin %1 can not create metadata manager for resource %2 (%3).")
                    .args(plugin->name(), camera->getUserDefinedName(), camera->getId()));
            continue;
        }
        nxpt::ScopedRef<AbstractMetadataManager> managerGuard(manager, false);

        boost::optional<nx::api::AnalyticsDriverManifest> pluginManifest =
            loadPluginManifest(plugin);
        if (!pluginManifest)
            return;
        addPluginManifestToServer(*pluginManifest, server);

        boost::optional<nx::api::AnalyticsDeviceManifest> managerManifest;
        boost::optional<nx::api::AnalyticsDriverManifest> auxiliaryPluginManifest;
        std::tie(managerManifest, auxiliaryPluginManifest) = loadManagerManifest(manager, camera);
        if (managerManifest)
        {
            addManifestToCamera(*managerManifest, camera);
        }
        if (auxiliaryPluginManifest)
        {
            auxiliaryPluginManifest->driverId = pluginManifest->driverId;
            addPluginManifestToServer(*auxiliaryPluginManifest, server);
        }

        auto handler = createMetadataHandler(camera, pluginManifest->driverId);
        if (!handler)
        {
            NX_DEBUG(
                this,
                lm("Unable to create metadata handler for resource %1 (%2).")
                    .args(camera->getUserDefinedName(), camera->getId()));
            continue;
        }

        NX_DEBUG(this, lm("Camera %1 got new manager %2.").args(cameraId, typeid(*manager)));
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
    NX_ASSERT(camera && plugin);
    if (!camera || !plugin)
        return nullptr;

    NX_DEBUG(
        this,
        lm("Creating metadata manager for resource %1 (%2).")
            .args(camera->getUserDefinedName(), camera->getId()));

    Error error = Error::noError;
    ResourceInfo resourceInfo;
    bool success = resourceInfoFromResource(camera, &resourceInfo);
    if (!success)
    {
        NX_WARNING(
            this,
            lm("Can not create resource info from resource %1 (%2)")
                .args(camera->getUserDefinedName(), camera->getId()));
        return nullptr;
    }

    NX_DEBUG(
        this,
        lm("Resource info for resource %1 (%2): %3")
        .args(camera->getUserDefinedName(), camera->getId(), resourceInfo));

    return plugin->managerForResource(resourceInfo, &error);
}

void ManagerPool::releaseResourceMetadataManagers(const QnSecurityCamResourcePtr& resource)
{
    const auto id = resource->getId();
    const auto removedCount = m_contexts.erase(id);
    NX_DEBUG(this, lm("Camera %1 lost all %2 managers %2").args(id, removedCount));
}

AbstractMetadataHandler* ManagerPool::createMetadataHandler(
    const QnResourcePtr& resource,
    const QnUuid& pluginId)
{
    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
    {
        NX_ERROR(
            this,
            lm("Resource %1 (%2) is not an instance of SecurityCameResource")
                .args(resource->getName(), resource->getId()));
        return nullptr;
    }

    auto handler = new EventHandler();
    handler->setResource(camera);
    handler->setPluginId(pluginId);

    return handler;
}

void ManagerPool::handleResourceChanges(const QnResourcePtr& resource)
{
    NX_VERBOSE(
        this,
        lm("Handling resource changes for resource %1 (%2)")
            .args(resource->getName(), resource->getId()));

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
    {
        return;
    }

    releaseResourceMetadataManagers(camera);
    if (canFetchMetadataFromResource(camera))
    {
        NX_DEBUG(
            this,
            lm("Creating metadata managers for resource %1 (%2).")
                .args(camera->getUserDefinedName(), camera->getId()));

        createMetadataManagersForResource(camera);
    }

    auto resourceId = camera->getId();
    auto events = qnServerModule
        ->metadataRuleWatcher()
        ->watchedEventsForResource(resourceId);

    fetchMetadataForResource(resourceId, events);
}

bool ManagerPool::canFetchMetadataFromResource(const QnSecurityCamResourcePtr& camera) const
{
    if (!camera)
        return false;

    const auto status = camera->getStatus();
    const auto flags = camera->flags();

    NX_VERBOSE(
        this,
        lm("Determining if metadata can be fetched from resource: "
            "is foreign resource: %1, "
            "is desktop camera: %2, "
            "is resource status ok: %3")
            .args(
                flags.testFlag(Qn::foreigner),
                flags.testFlag(Qn::desktop_camera),
                (status == Qn::Online || status == Qn::Recording)));

    return !flags.testFlag(Qn::foreigner)
        && !flags.testFlag(Qn::desktop_camera)
        && (status == Qn::Online || status == Qn::Recording);
}

bool ManagerPool::fetchMetadataForResource(const QnUuid& resourceId, QSet<QnUuid>& eventTypeIds)
{
    // FIXME: Currently we always use a single random plugin for each resource on each request.
    auto context = m_contexts.find(resourceId);
    if (context == m_contexts.cend())
    {
        NX_ERROR(
            this,
            lm("Could not find metadata context for resource %1")
                .arg(resourceId));

        return false;
    }

    auto& manager = context->second.manager;
    auto& handler = context->second.handler;
    Error result = Error::unknownError;

    if (eventTypeIds.empty())
    {
        NX_DEBUG(
            this,
            lm("Event list is empty, stopping metdata fetching for resource %1.")
                .arg(resourceId));

        result = manager->stopFetchingMetadata();
    }
    else
    {
        NX_DEBUG(
            this,
            lm("Starting metadata fetching for resource %1. Event list is %2")
                .args(resourceId, eventTypeIds));

        std::vector<nxpl::NX_GUID> eventTypeList;
        for (const auto& eventTypeId: eventTypeIds)
            eventTypeList.push_back(nxpt::NxGuidHelper::fromRawData(eventTypeId.toRfc4122()));
        result = manager->startFetchingMetadata(
            handler.get(),
            !eventTypeList.empty() ? &eventTypeList[0] : nullptr,
            static_cast<int>(eventTypeList.size()));
    }

    return result == Error::noError;
}

uint qHash(const nx::api::AnalyticsEventType& t)// noexcept
{
    return qHash(t.eventTypeId.toByteArray());
}

boost::optional<nx::api::AnalyticsDriverManifest> ManagerPool::loadPluginManifest(
    AbstractMetadataPlugin* plugin)
{
    Error error = Error::noError;
    auto pluginManifest = deserializeManifest<nx::api::AnalyticsDriverManifest>(
        plugin->capabilitiesManifest(&error));

    if (!pluginManifest || error != Error::noError)
    {
        NX_ERROR(
            this,
            lm("Can not deserialize plugin manifest from plugin %1").arg(plugin->name()));
    }
    return pluginManifest;
}

void ManagerPool::addPluginManifestToServer(
    const nx::api::AnalyticsDriverManifest& manifest,
    const QnMediaServerResourcePtr& server)
{
    bool expanded = false;
    auto existingManifests = server->analyticsDrivers();
    for (auto& existingManifest : existingManifests)
    {
        if (existingManifest.driverId == manifest.driverId)
        {
//#define THIS_IS_FOR_TESTING_PURPOSES_ONLY__SERVER
#ifdef THIS_IS_FOR_TESTING_PURPOSES_ONLY__SERVER
            existingManifest.outputEventTypes.clear();
#else
            existingManifest.outputEventTypes = existingManifest.outputEventTypes.toSet().
                unite(manifest.outputEventTypes.toSet()).toList();
#endif
            expanded = true;
            break;
        }
    }

    if (!expanded)
        existingManifests.push_back(manifest);

    server->setAnalyticsDrivers(existingManifests);
    server->saveParams();
}

std::pair<
    boost::optional<nx::api::AnalyticsDeviceManifest>,
    boost::optional<nx::api::AnalyticsDriverManifest>
    >
ManagerPool::loadManagerManifest(
    AbstractMetadataManager* manager,
    const QnSecurityCamResourcePtr& camera)
{
    NX_ASSERT(manager);
    NX_ASSERT(camera);

    Error error = Error::noError;

    // "managerManifest" contains const char* representation of camera manifest.
    // unique_ptr allows us to automatically ask manager to release it when memory is not needed more.
    auto deleter = [manager](const char* ptr) { manager->freeManifest(ptr); };
    std::unique_ptr<const char, decltype(deleter)> managerManifest(
        manager->capabilitiesManifest(&error), deleter);

    if (error != Error::noError)
    {
        NX_ERROR(
            this,
            lm("Can not fetch manifest for resource %1 (%2), plugin returned error.")
            .args(camera->getUserDefinedName(), camera->getId()));
        return std::make_pair(boost::none, boost::none);
    }

    // Manager::capabilitiesManifest can return data in two json formats: either
    // AnalyticsDeviceManifest or AnalyticsDriverManifest.
    // First we try AnalyticsDeviceManifest.
    auto deviceManifest = deserializeManifest<nx::api::AnalyticsDeviceManifest>(
        managerManifest.get());

    if (deviceManifest && deviceManifest->supportedEventTypes.size())
        return std::make_pair(deviceManifest, boost::none);

    // If manifest occured to be not AnalyticsDeviceManifest, we try to treat it as
    // AnalyticsDriverManifest.
    auto driverManifest = deserializeManifest<nx::api::AnalyticsDriverManifest>(
        managerManifest.get());

    if (driverManifest && driverManifest->outputEventTypes.size())
    {
        deviceManifest = nx::api::AnalyticsDeviceManifest();
        std::transform(
            driverManifest->outputEventTypes.cbegin(),
            driverManifest->outputEventTypes.cend(),
            std::back_inserter(deviceManifest->supportedEventTypes),
            [](const nx::api::AnalyticsEventType& driverManifestElement)
            {
                return driverManifestElement.eventTypeId;
            });
        return std::make_pair(deviceManifest, driverManifest);
    }

    // If manifest format is invalid.
    NX_ERROR(
        this,
        lm("Can not deserialize manifest for resource %1 (%2)")
        .args(camera->getUserDefinedName(), camera->getId()));
    return std::make_pair(boost::none, boost::none);
}

void ManagerPool::addManifestToCamera(
    const nx::api::AnalyticsDeviceManifest& manifest,
    const QnSecurityCamResourcePtr& camera)
{
    camera->setAnalyticsSupportedEvents(manifest.supportedEventTypes);
    camera->saveParams();
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

} // namespace metadata
} // namespace mediaserver
} // namespace nx
