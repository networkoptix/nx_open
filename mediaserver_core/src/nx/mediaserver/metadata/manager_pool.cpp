#include "manager_pool.h"

#include <common/common_module.h>
#include <media_server/media_server_module.h>

#include <plugins/plugin_manager.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_internal_tools.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/fusion/serialization/json.h>
#include <nx/mediaserver/metadata/metadata_handler.h>
#include <nx/mediaserver/metadata/event_rule_watcher.h>

#include <nx/api/analytics/device_manifest.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/sdk/metadata/abstract_consuming_metadata_manager.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include "media_data_receptor.h"
#include <nx/utils/log/log_main.h>

#include <ini.h>

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

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

    QnMutexLocker lock(&m_contextMutex);
    m_contexts.erase(resource->getId());
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
    NX_ASSERT(pluginManager, lit("Cannot access PluginManager instance"));
    if (!pluginManager)
        return PluginList();

    return pluginManager->findNxPlugins<AbstractMetadataPlugin>(
        IID_MetadataPlugin);
}

void ManagerPool::createMetadataManagersForResourceUnsafe(const QnSecurityCamResourcePtr& camera)
{
    if (ini().enablePersistentMetadataManager)
    {
        const auto itr = m_contexts.find(camera->getId());
        if (itr != m_contexts.cend())
        {
            if (!itr->second.managers().empty())
                return;
        }
    }

    for (AbstractMetadataPlugin* const plugin: availablePlugins())
    {
        nxpt::ScopedRef<AbstractMetadataPlugin> pluginGuard(plugin, /*increaseRef*/ false);

        nxpt::ScopedRef<AbstractMetadataManager> manager(
            createMetadataManager(camera, plugin), /*increaseRef*/ false);
        if (!manager)
            continue;

        const auto pluginManifest = addManifestToServer(plugin);
        if (!pluginManifest)
            continue; //< Error already logged.

        const auto deviceManifest = addManifestToCamera(camera, manager.get());
        if (!deviceManifest)
            continue; //< Error already logged.

        auto& context = m_contexts[camera->getId()];
        std::unique_ptr<MetadataHandler> handler(
            createMetadataHandler(camera, pluginManifest->driverId));

        if (auto consumingMetadataManager = nxpt::ScopedRef<AbstractConsumingMetadataManager>(
            manager->queryInterface(IID_ConsumingMetadataManager)))
        {
            handler->registerDataReceptor(&context);
        }

        context.addManager(std::move(manager), std::move(handler), *pluginManifest);
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
            lm("Cannot create resource info from resource %1 (%2)")
                .args(camera->getUserDefinedName(), camera->getId()));
        return nullptr;
    }

    NX_DEBUG(
        this,
        lm("Resource info for resource %1 (%2): %3")
        .args(camera->getUserDefinedName(), camera->getId(), resourceInfo));

    return plugin->managerForResource(resourceInfo, &error);
}

void ManagerPool::releaseResourceMetadataManagersUnsafe(const QnSecurityCamResourcePtr& camera)
{
    if (ini().enablePersistentMetadataManager)
        return;

    auto& context = m_contexts[camera->getId()];
    context.clearManagers();
    context.setManagersInitialized(false);
}

MetadataHandler* ManagerPool::createMetadataHandler(
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

    auto handler = new MetadataHandler();
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

    {
        NX_DEBUG(
            this,
            lm("Creating metadata managers for resource %1 (%2).")
                .args(camera->getUserDefinedName(), camera->getId()));

    }
    auto resourceId = camera->getId();
    QnMutexLocker lock(&m_contextMutex);
    auto& context = m_contexts[resourceId];
    if (isCameraAlive(camera))
    {
        if (!context.isManagerInitialized())
            createMetadataManagersForResourceUnsafe(camera);
        auto events = qnServerModule->metadataRuleWatcher()->watchedEventsForResource(resourceId);
        fetchMetadataForResourceUnsafe(resourceId, context, events);
    }
    else
    {
        releaseResourceMetadataManagersUnsafe(camera);
    }
}

bool ManagerPool::isCameraAlive(const QnSecurityCamResourcePtr& camera) const
{
    if (!camera)
        return false;

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
                (camera->getStatus() == Qn::Online || camera->getStatus() == Qn::Recording)));

    if (flags.testFlag(Qn::foreigner) || flags.testFlag(Qn::desktop_camera))
        return false;

    return camera->getStatus() >= Qn::Online;
}

void ManagerPool::fetchMetadataForResourceUnsafe(
    const QnUuid& resourceId,
    ResourceMetadataContext& context,
    QSet<QnUuid>& eventTypeIds)
{
    for (auto& data: context.managers())
    {
        if (eventTypeIds.empty())
        {
            NX_DEBUG(
                this,
                lm("Event list is empty, stopping metdata fetching for resource %1.")
                    .arg(resourceId));

            auto result = data.manager->stopFetchingMetadata();
            if (result != Error::noError)
                NX_WARNING(this, lm("Failed to stop fetching metadata from plugin %1").arg(data.manifest.driverName.value));
        }
        else
        {
            NX_DEBUG(
                this,
                lm("Starting metadata fetching for resource %1. Event list is %2")
                    .args(resourceId, eventTypeIds));

            auto result = data.manager->startFetchingMetadata(); //< TODO: #dmishin pass event types.

            if (result != Error::noError)
                NX_WARNING(this, lm("Failed to stop fetching metadata from plugin %1").arg(data.manifest.driverName.value));
        }
    }
}

boost::optional<nx::api::AnalyticsDriverManifest> ManagerPool::addManifestToServer(
    AbstractMetadataPlugin* plugin)
{
    auto server = m_serverModule
        ->commonModule()
        ->resourcePool()
        ->getResourceById<QnMediaServerResource>(
            m_serverModule->commonModule()->moduleGUID());

    NX_ASSERT(server, lm("Cannot obtain current server resource."));
    if (!server)
        return boost::none;

    Error error = Error::noError;
    const char* const manifestStr = plugin->capabilitiesManifest(&error);
    if (error != Error::noError)
    {
        NX_ERROR(this) << lm("Unable to receive Plugin manifest for \"%1\": %2.")
            .args(plugin->name(), error);
        return boost::none;
    }
    if (manifestStr == nullptr)
    {
        NX_ERROR(this) << lm("Received null Plugin manifest for \"%1\".")
            .arg(plugin->name());
        return boost::none;
    }

    auto manifest = deserializeManifest<nx::api::AnalyticsDriverManifest>(manifestStr);
    if (!manifest)
    {
        NX_ERROR(
            this,
            lm("Cannot deserialize plugin manifest from plugin %1").arg(plugin->name()));
        return boost::none; //< Error already logged.
    }

    bool overwritten = false;
    auto existingManifests = server->analyticsDrivers();
    for (auto& existingManifest : existingManifests)
    {
        if (existingManifest.driverId == manifest->driverId)
        {
            existingManifest = *manifest;
            overwritten = true;
            break;
        }
    }

    if (!overwritten)
        existingManifests.push_back(*manifest);

    server->setAnalyticsDrivers(existingManifests);
    server->saveParams();

    return manifest;
}

boost::optional<nx::api::AnalyticsDeviceManifest> ManagerPool::addManifestToCamera(
    const QnSecurityCamResourcePtr& camera,
    AbstractMetadataManager* manager)
{
    NX_ASSERT(manager);
    NX_ASSERT(camera);

    Error error = Error::noError;
    const char *const manifestStr = manager->capabilitiesManifest(&error);
    if (error != Error::noError)
    {
        NX_ERROR(this) << lm("Unable to receive Manager manifest for \"%1\": %2.")
            .args(manager->plugin()->name(), error);
        return boost::none;
    }
    if (manifestStr == nullptr)
    {
        NX_ERROR(this) << lm("Received null Manager manifest for \"%1\".")
            .arg(manager->plugin()->name());
        return boost::none;
    }

    auto manifest = deserializeManifest<nx::api::AnalyticsDeviceManifest>(manifestStr);
    if (!manifest)
    {
        NX_ERROR(
            this,
            lm("Cannot fetch or deserialize manifest for resource %1 (%2)")
                .args(camera->getUserDefinedName(), camera->getId()));
        return boost::none; //< Error already logged.
    }
    camera->setAnalyticsSupportedEvents(manifest->supportedEventTypes);
    camera->saveParams();

    return manifest;
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

void ManagerPool::putVideoData(const QnUuid& id, const QnCompressedVideoData* video)
{
    QnMutexLocker lock(&m_contextMutex);
    for (auto& data: m_contexts[id].managers())
    {
        using namespace nx::sdk::metadata;
        nxpt::ScopedRef<AbstractConsumingMetadataManager> manager(
            data.manager->queryInterface(IID_ConsumingMetadataManager));
        if (!manager)
            return;
        bool needDeepCopy = data.manifest.capabilities.testFlag(
            nx::api::AnalyticsDriverManifestBase::needDeepCopyForMediaFrame);
        auto packet = toPluginVideoPacket(video, needDeepCopy);
        manager->putData(packet.get());
    }
}

QWeakPointer<QnAbstractDataReceptor> ManagerPool::mediaDataReceptor(const QnUuid& id)
{
    QnMutexLocker lock(&m_contextMutex);
    auto& context = m_contexts[id];
    auto dataReceptor = VideoDataReceptorPtr(new VideoDataReceptor(
        std::bind(&ManagerPool::putVideoData, this, id, std::placeholders::_1)));
    context.setVideoFrameDataReceptor(dataReceptor);
    return dataReceptor.toWeakRef();
}

void ManagerPool::registerDataReceptor(
    const QnResourcePtr& resource,
    QWeakPointer<QnAbstractDataReceptor> metadaReceptor)
{
    QnMutexLocker lock(&m_contextMutex);
    const auto& id = resource->getId();

    auto& context = m_contexts[id];
    context.setMetadataDataReceptor(metadaReceptor);
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
