// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtCore/QPointer>
#include <QtCore/QThreadPool>

#include <api/common_message_processor.h>
#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <core/resource/camera_history.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/providers/shared_layout_item_access_provider.h>
#include <core/resource_access/providers/shared_resource_access_provider.h>
#include <core/resource_access/providers/videowall_item_access_provider.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource_management/user_roles_manager.h>
#include <licensing/license.h>
#include <network/router.h>
#include <nx/analytics/engine_descriptor_manager.h>
#include <nx/analytics/event_type_descriptor_manager.h>
#include <nx/analytics/group_descriptor_manager.h>
#include <nx/analytics/object_type_descriptor_manager.h>
#include <nx/analytics/plugin_descriptor_manager.h>
#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/analytics/taxonomy/state_watcher.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/rules/ec2_router.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/initializer.h>

namespace nx::vms::common {

using namespace nx::analytics;

struct SystemContext::Private
{
    const QnUuid peerId;
    /*const*/ QnUuid sessionId; //< FIXME: #sivanov Make separate sessions with own ids.
    QnCommonMessageProcessor* messageProcessor = nullptr;

    std::unique_ptr<QnLicensePool> licensePool;
    std::unique_ptr<QnResourcePool> resourcePool;
    std::unique_ptr<QnResourceDataPool> resourceDataPool;
    std::unique_ptr<QnResourceStatusDictionary> resourceStatusDictionary;
    std::unique_ptr<QnResourcePropertyDictionary> resourcePropertyDictionary;
    std::unique_ptr<QnCameraHistoryPool> cameraHistoryPool;
    std::unique_ptr<QnServerAdditionalAddressesDictionary> serverAdditionalAddressesDictionary;
    std::unique_ptr<QnRuntimeInfoManager> runtimeInfoManager;
    std::unique_ptr<QnGlobalSettings> globalSettings;
    std::unique_ptr<QnUserRolesManager> userRolesManager;
    std::unique_ptr<QnSharedResourcesManager> sharedResourceManager;
    std::unique_ptr<QnGlobalPermissionsManager> globalPermissionsManager;
    std::unique_ptr<QnResourceAccessSubjectsCache> resourceAccessSubjectCache;
    std::unique_ptr<nx::core::access::ResourceAccessProvider> resourceAccessProvider;
    std::unique_ptr<QnResourceAccessManager> resourceAccessManager;
    std::unique_ptr<QnLayoutTourManager> layoutTourManager;
    std::unique_ptr<nx::vms::event::RuleManager> eventRuleManager;
    std::unique_ptr<nx::vms::rules::Engine> vmsRulesEngine;
    std::unique_ptr<taxonomy::DescriptorContainer> analyticsDescriptorContainer;
    std::unique_ptr<taxonomy::AbstractStateWatcher> analyticsTaxonomyStateWatcher;
    std::unique_ptr<PluginDescriptorManager> analyticsPluginDescriptorManager;
    std::unique_ptr<EventTypeDescriptorManager> analyticsEventTypeDescriptorManager;
    std::unique_ptr<EngineDescriptorManager> analyticsEngineDescriptorManager;
    std::unique_ptr<GroupDescriptorManager> analyticsGroupDescriptorManager;
    std::unique_ptr<ObjectTypeDescriptorManager> analyticsObjectTypeDescriptorManager;

    QPointer<QnRouter> router;
    QPointer<AbstractCertificateVerifier>  certificateVerifier;
};

SystemContext::SystemContext(
    QnUuid peerId,
    QnUuid sessionId,
    nx::core::access::Mode resourceAccessMode)
    :
    d(new Private{.peerId = std::move(peerId), .sessionId=std::move(sessionId)})
{
    d->licensePool = std::make_unique<QnLicensePool>(this);
    d->resourcePool = std::make_unique<QnResourcePool>(this);
    d->resourceDataPool = std::make_unique<QnResourceDataPool>();
    d->resourceStatusDictionary = std::make_unique<QnResourceStatusDictionary>();
    d->resourcePropertyDictionary = std::make_unique<QnResourcePropertyDictionary>(this);
    d->cameraHistoryPool = std::make_unique<QnCameraHistoryPool>(this);
    d->serverAdditionalAddressesDictionary =
        std::make_unique<QnServerAdditionalAddressesDictionary>();
    d->runtimeInfoManager = std::make_unique<QnRuntimeInfoManager>(this);
    d->globalSettings = std::make_unique<QnGlobalSettings>(this);
    d->userRolesManager = std::make_unique<QnUserRolesManager>(this);

    // Depends on resource pool and roles.
    d->sharedResourceManager = std::make_unique<QnSharedResourcesManager>(this);

    // Depends on resource pool and roles.
    d->globalPermissionsManager =
        std::make_unique<QnGlobalPermissionsManager>(resourceAccessMode, this);

    // Depends on resource pool, roles and global permissions.
    d->resourceAccessSubjectCache = std::make_unique<QnResourceAccessSubjectsCache>(this);

    // Depends on resource pool, roles and shared resources.
    d->resourceAccessProvider =
        std::make_unique<nx::core::access::ResourceAccessProvider>(resourceAccessMode, this);

    // Some of base providers depend on QnGlobalPermissionsManager and QnSharedResourcesManager.
    d->resourceAccessProvider->addBaseProvider(
        new nx::core::access::PermissionsResourceAccessProvider(resourceAccessMode, this));
    d->resourceAccessProvider->addBaseProvider(
        new nx::core::access::SharedResourceAccessProvider(resourceAccessMode, this));
    d->resourceAccessProvider->addBaseProvider(
        new nx::core::access::SharedLayoutItemAccessProvider(resourceAccessMode, this));
    d->resourceAccessProvider->addBaseProvider(
        new nx::core::access::VideoWallItemAccessProvider(resourceAccessMode, this));

    // Depends on access provider.
    d->resourceAccessManager = std::make_unique<QnResourceAccessManager>(resourceAccessMode, this);

    d->layoutTourManager = std::make_unique<QnLayoutTourManager>();
    d->eventRuleManager = std::make_unique<nx::vms::event::RuleManager>();
    d->vmsRulesEngine = std::make_unique<nx::vms::rules::Engine>(
        std::make_unique<nx::vms::rules::Ec2Router>(this));
    nx::vms::rules::Initializer initializer(d->vmsRulesEngine.get());
    initializer.registerFields();
    initializer.registerEvents();
    initializer.registerActions();

    d->analyticsDescriptorContainer = std::make_unique<taxonomy::DescriptorContainer>(this);
    d->analyticsTaxonomyStateWatcher = std::make_unique<taxonomy::StateWatcher>(
        d->analyticsDescriptorContainer.get());
    d->analyticsPluginDescriptorManager = std::make_unique<PluginDescriptorManager>(
        d->analyticsDescriptorContainer.get());
    d->analyticsEventTypeDescriptorManager = std::make_unique<EventTypeDescriptorManager>(
        d->analyticsDescriptorContainer.get());
    d->analyticsEngineDescriptorManager = std::make_unique<EngineDescriptorManager>(
        d->analyticsDescriptorContainer.get());
    d->analyticsGroupDescriptorManager = std::make_unique<GroupDescriptorManager>(
        d->analyticsDescriptorContainer.get());
    d->analyticsObjectTypeDescriptorManager = std::make_unique<ObjectTypeDescriptorManager>(
        d->analyticsDescriptorContainer.get());
}

SystemContext::~SystemContext()
{
    d->resourcePool->threadPool()->waitForDone();
    d->resourceAccessProvider->clear();
}

void SystemContext::initNetworking(
    QnRouter* router,
    AbstractCertificateVerifier* certificateVerifier)
{
    d->router = router;
    d->certificateVerifier = certificateVerifier;
}

const QnUuid& SystemContext::peerId() const
{
    return d->peerId;
}

const QnUuid& SystemContext::sessionId() const
{
    return d->sessionId;
}

void SystemContext::updateRunningInstanceGuid()
{
    d->sessionId = QnUuid::createUuid();
}

QnRouter* SystemContext::router() const
{
    return d->router;
}

AbstractCertificateVerifier* SystemContext::certificateVerifier() const
{
    return d->certificateVerifier;
}

std::shared_ptr<ec2::AbstractECConnection> SystemContext::ec2Connection() const
{
    if (d->messageProcessor)
        return d->messageProcessor->connection();

    return nullptr;
}

QnCommonMessageProcessor* SystemContext::messageProcessor() const
{
    return d->messageProcessor;
}

void SystemContext::deleteMessageProcessor()
{
    if (!NX_ASSERT(d->messageProcessor))
        return;

    d->messageProcessor->init(nullptr); // stop receiving notifications
    runtimeInfoManager()->setMessageProcessor(nullptr);
    cameraHistoryPool()->setMessageProcessor(nullptr);
    delete d->messageProcessor;
    d->messageProcessor = nullptr;
}

QnLicensePool* SystemContext::licensePool() const
{
    return d->licensePool.get();
}

QnResourcePool* SystemContext::resourcePool() const
{
    return d->resourcePool.get();
}

QnResourceDataPool* SystemContext::resourceDataPool() const
{
    return d->resourceDataPool.get();
}

QnResourceStatusDictionary* SystemContext::resourceStatusDictionary() const
{
    return d->resourceStatusDictionary.get();
}

QnResourcePropertyDictionary* SystemContext::resourcePropertyDictionary() const
{
    return d->resourcePropertyDictionary.get();
}

QnCameraHistoryPool* SystemContext::cameraHistoryPool() const
{
    return d->cameraHistoryPool.get();
}

QnServerAdditionalAddressesDictionary* SystemContext::serverAdditionalAddressesDictionary() const
{
    return d->serverAdditionalAddressesDictionary.get();
}

QnRuntimeInfoManager* SystemContext::runtimeInfoManager() const
{
    return d->runtimeInfoManager.get();
}

QnGlobalSettings* SystemContext::globalSettings() const
{
    return d->globalSettings.get();
}

QnUserRolesManager* SystemContext::userRolesManager() const
{
    return d->userRolesManager.get();
}

QnSharedResourcesManager* SystemContext::sharedResourcesManager() const
{
    return d->sharedResourceManager.get();
}

QnResourceAccessManager* SystemContext::resourceAccessManager() const
{
    return d->resourceAccessManager.get();
}

QnGlobalPermissionsManager* SystemContext::globalPermissionsManager() const
{
    return d->globalPermissionsManager.get();
}

QnResourceAccessSubjectsCache* SystemContext::resourceAccessSubjectsCache() const
{
    return d->resourceAccessSubjectCache.get();
}

nx::core::access::ResourceAccessProvider* SystemContext::resourceAccessProvider() const
{
    return d->resourceAccessProvider.get();
}

QnLayoutTourManager* SystemContext::layoutTourManager() const
{
    return d->layoutTourManager.get();
}

nx::vms::event::RuleManager* SystemContext::eventRuleManager() const
{
    return d->eventRuleManager.get();
}

nx::vms::rules::Engine* SystemContext::vmsRulesEngine() const
{
    return d->vmsRulesEngine.get();
}

PluginDescriptorManager* SystemContext::analyticsPluginDescriptorManager() const
{
    return d->analyticsPluginDescriptorManager.get();
}

EventTypeDescriptorManager*
    SystemContext::analyticsEventTypeDescriptorManager() const
{
    return d->analyticsEventTypeDescriptorManager.get();
}

EngineDescriptorManager* SystemContext::analyticsEngineDescriptorManager() const
{
    return d->analyticsEngineDescriptorManager.get();
}

GroupDescriptorManager* SystemContext::analyticsGroupDescriptorManager() const
{
    return d->analyticsGroupDescriptorManager.get();
}

ObjectTypeDescriptorManager*
    SystemContext::analyticsObjectTypeDescriptorManager() const
{
    return d->analyticsObjectTypeDescriptorManager.get();
}

taxonomy::DescriptorContainer* SystemContext::analyticsDescriptorContainer() const
{
    return d->analyticsDescriptorContainer.get();
}

taxonomy::AbstractStateWatcher*
    SystemContext::analyticsTaxonomyStateWatcher() const
{
    return d->analyticsTaxonomyStateWatcher.get();
}

void SystemContext::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    d->messageProcessor = messageProcessor;
    runtimeInfoManager()->setMessageProcessor(messageProcessor);
    cameraHistoryPool()->setMessageProcessor(messageProcessor);
}

} // namespace nx::vms::common
