#include "manager_pool.h"

#include <common/common_module.h>
#include <media_server/media_server_module.h>
#include <plugins/plugin_manager.h>
#include <plugins/plugin_tools.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>

#include <nx/fusion/serialization/json.h>
#include <nx/mediaserver/metadata/event_handler.h>
#include <nx/mediaserver/metadata/event_rule_watcher.h>
#include <nx/api/analytics/supported_events.h>


namespace nx {
namespace mediaserver {
namespace metadata {

namespace metadata_sdk = nx::sdk::metadata;

ResourceMetadataContext::ResourceMetadataContext(
    metadata_sdk::AbstractMetadataManager* metadataManager,
    metadata_sdk::AbstractMetadataHandler* metadataHandler)
    :
    manager(
        metadataManager,
        ManagerDeleter(
            [](metadata_sdk::AbstractMetadataManager* metadataManager)
            {
                metadataManager->stopFetchingMetadata();
                metadataManager->releaseRef();
            })),
    handler(metadataHandler)
{
}

ManagerPool::ManagerPool(QnCommonModule* commonModule):
    nx::core::resource::ResourcePoolAware(commonModule)
{
    QObject::connect(
        qnServerModule->metadataRuleWatcher(), &EventRuleWatcher::rulesUpdated,
        this, &ManagerPool::rulesUpdated);
}

void ManagerPool::resourceAdded(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    QObject::connect(
        resource.data(), &QnResource::statusChanged,
        this, &ManagerPool::handleResourceChanges);

    QObject::connect(
        resource.data(), &QnResource::parentIdChanged,
        this, &ManagerPool::handleResourceChanges);

    handleResourceChangesUnsafe(resource);
}

void ManagerPool::resourceRemoved(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    releaseResourceMetadataManagers(resource);
}

void ManagerPool::rulesUpdated(const std::set<QnUuid>& affectedResources)
{
    QnMutexLocker lock(&m_mutex);
    for (const auto& resourceId: affectedResources)
    {
        handleResourceChangesUnsafe(
            commonModule()
                ->resourcePool()
                ->getResourceById(resourceId));
    }
}

metadata_sdk::AbstractMetadataHandler* ManagerPool::createMetadataHandler(
    const QnResourcePtr& resource,
    const nx::api::AnalyticsDriverManifest& manifest)
{
    auto handler = new EventHandler();
    handler->setResource(resource);
    handler->setPluginId(manifest.driverId);

    return handler;
}

void ManagerPool::createMetadataManagersForResource(const QnResourcePtr& resource)
{
    auto resourceId = resource->getId();
    if (!canFetchMetadataFromResource(resourceId))
        return;

    releaseResourceMetadataManagers(resource);  

    auto pluginManager = qnServerModule->pluginManager();
    NX_ASSERT(pluginManager, lit("Can not access PluginManager instance"));
    if (!pluginManager)
        return;

    auto plugins = pluginManager->findNxPlugins<metadata_sdk::AbstractMetadataPlugin>(
        metadata_sdk::IID_MetadataPlugin);

    for (auto& plugin: plugins)
    {
        metadata_sdk::Error error = metadata_sdk::Error::noError;
        metadata_sdk::ResourceInfo resourceInfo;
        bool success = resourceInfoFromResource(resource, &resourceInfo);
        if (!success)
            return;

        nxpt::ScopedRef<metadata_sdk::AbstractMetadataManager> manager(
            plugin->managerForResource(resourceInfo, &error));

        if (!manager)
            continue;

        auto manifest = deserializeManifest(manager->capabiltiesManifest());
        if (!manifest.is_initialized())
            continue;

        auto server = commonModule()
            ->resourcePool()
            ->getResourceById(commonModule()->moduleGUID())
                .dynamicCast<QnMediaServerResource>();

        if (!server)
            return;

        server->setAnalyticsDrivers({*manifest});

        auto nonConstResource = commonModule()
            ->resourcePool()
            ->getResourceById(resource->getId());

        if (!nonConstResource)
            return;
        
        auto secRes = nonConstResource.dynamicCast<QnSecurityCamResource>();

        nx::api::AnalyticsSupportedEvents analyticsSupportedEvents;
        analyticsSupportedEvents.driverId = manifest->driverId;
        for (const auto& event: manifest->outputEventTypes)
            analyticsSupportedEvents.eventTypes.push_back(event.eventTypeId);

        secRes->setAnalyticsSupportedEvents({analyticsSupportedEvents});

        auto handler = createMetadataHandler(secRes, *manifest);
        if (!handler)
            continue;

        m_contexts.emplace(
            resource->getId(),
            ResourceMetadataContext(manager.get(), handler));
    }
}

void ManagerPool::releaseResourceMetadataManagers(const QnResourcePtr& resource)
{
    auto id = resource->getId();
    auto range  = m_contexts.erase(id);
}


void ManagerPool::handleResourceChanges(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    handleResourceChangesUnsafe(resource);
}

void ManagerPool::handleResourceChangesUnsafe(const QnResourcePtr& resource)
{
    auto resourceId = resource->getId();
    auto& events = qnServerModule
        ->metadataRuleWatcher()
        ->watchedEventsForResource(resourceId);

    if (canFetchMetadataFromResource(resourceId))
    {
        auto context = m_contexts.find(resourceId);
        if (context == m_contexts.cend())
            createMetadataManagersForResource(resource);
    }

    fetchMetadataForResource(resourceId, events);
}

bool ManagerPool::canFetchMetadataFromResource(const QnUuid& resourceId) const
{
    auto resource = commonModule()
        ->resourcePool()
        ->getResourceById(resourceId);

    if (!resource)
        return false;

    auto status = resource->getStatus();
    return !resource->hasFlags(Qn::foreigner | Qn::desktop_camera)
        && (status == Qn::Online || status == Qn::Recording)
        && resource.dynamicCast<QnSecurityCamResource>();
}

bool ManagerPool::fetchMetadataForResource(const QnUuid& resourceId, std::set<QnUuid>& eventTypeIds)
{
    auto context = m_contexts.find(resourceId);
    if (context == m_contexts.cend())
        return false;

    auto& manager = context->second.manager;
    auto& handler = context->second.handler;
    metadata_sdk::Error result = metadata_sdk::Error::unknownError;

    if (eventTypeIds.empty())
        result = manager->stopFetchingMetadata();
    else
        result = manager->startFetchingMetadata(handler.get()); //< TODO: #dmishin pass event types.

    return result == metadata_sdk::Error::noError;
}

bool ManagerPool::resourceInfoFromResource(
    const QnResourcePtr& resource,
    metadata_sdk::ResourceInfo* outResourceInfo) const
{
    auto secRes = resource.dynamicCast<QnSecurityCamResource>();
    if (!secRes)
        return false;

    strncpy(
        outResourceInfo->vendor,
        secRes->getVendor().toUtf8().data(),
        metadata_sdk::ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->model,
        secRes->getModel().toUtf8().data(),
        metadata_sdk::ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->firmware,
        secRes->getFirmware().toUtf8().data(),
        metadata_sdk::ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->uid,
        secRes->getId().toByteArray().data(),
        metadata_sdk::ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->url,
        secRes->getUrl().toUtf8().data(),
        metadata_sdk::ResourceInfo::kTextParameterMaxLength);

    auto auth = secRes->getAuth();
    strncpy(
        outResourceInfo->login,
        auth.user().toUtf8().data(),
        metadata_sdk::ResourceInfo::kStringParameterMaxLength);

    strncpy(
        outResourceInfo->password,
        auth.password().toUtf8().data(),
        metadata_sdk::ResourceInfo::kStringParameterMaxLength);

    return true;
}

boost::optional<nx::api::AnalyticsDriverManifest> ManagerPool::deserializeManifest(
    const char* manifestString) const
{
    bool success = false;
    auto deserialized =  QJson::deserialized<nx::api::AnalyticsDriverManifest>(
        QString(manifestString).toUtf8(),
        nx::api::AnalyticsDriverManifest(),
        &success);

    if (!success)
        return boost::none;

    return deserialized;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
