// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtCore/QPointer>
#include <QtCore/QThreadPool>

#include <api/common_message_processor.h>
#include <api/runtime_info_manager.h>
#include <core/resource/camera_history.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/deprecated_access_rights_converter.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/providers/intercom_layout_access_provider.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/providers/shared_layout_item_access_provider.h>
#include <core/resource_access/providers/shared_resource_access_provider.h>
#include <core/resource_access/providers/videowall_item_access_provider.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
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
#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/analytics/taxonomy/state_watcher.h>
#include <nx/vms/common/license/license_usage_watcher.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/event/rule_manager.h>

namespace nx::vms::common {

using namespace nx::analytics;
using namespace nx::core::access;

struct SystemContext::Private
{
    const Mode mode;
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
    std::unique_ptr<SystemSettings> globalSettings;
    std::unique_ptr<QnUserRolesManager> userRolesManager;
    std::unique_ptr<QnSharedResourcesManager> sharedResourceManager;
    std::unique_ptr<ResourceAccessSubjectHierarchy> accessSubjectHierarchy;
    std::unique_ptr<GlobalPermissionsWatcher> globalPermissionsWatcher;
    std::unique_ptr<AccessRightsManager> accessRightsManager;
    std::unique_ptr<QnGlobalPermissionsManager> globalPermissionsManager;
    std::unique_ptr<QnResourceAccessSubjectsCache> resourceAccessSubjectCache;
    std::unique_ptr<DeprecatedAccessRightsConverter> deprecatedAccessRightsConverter;
    std::unique_ptr<ResourceAccessProvider> resourceAccessProvider;
    std::unique_ptr<QnResourceAccessManager> resourceAccessManager;
    std::unique_ptr<QnLayoutTourManager> showreelManager;
    std::unique_ptr<nx::vms::event::RuleManager> eventRuleManager;
    std::unique_ptr<taxonomy::DescriptorContainer> analyticsDescriptorContainer;
    std::unique_ptr<taxonomy::AbstractStateWatcher> analyticsTaxonomyStateWatcher;
    std::unique_ptr<DeviceLicenseUsageWatcher> deviceLicenseUsageWatcher;
    std::unique_ptr<VideoWallLicenseUsageWatcher> videoWallLicenseUsageWatcher;

    QPointer<AbstractCertificateVerifier> certificateVerifier;
    QPointer<nx::vms::discovery::Manager> moduleDiscoveryManager;
    nx::vms::rules::Engine* vmsRulesEngine = {};
};

SystemContext::SystemContext(
    Mode mode,
    QnUuid peerId,
    QnUuid sessionId,
    nx::core::access::Mode resourceAccessMode,
    QObject* parent)
    :
    QObject(parent),
    d(new Private{
        .mode = mode,
        .peerId = std::move(peerId),
        .sessionId=std::move(sessionId)
    })
{
    d->licensePool = std::make_unique<QnLicensePool>(this);
    d->resourcePool = std::make_unique<QnResourcePool>(this);
    d->resourceDataPool = std::make_unique<QnResourceDataPool>();
    d->resourceStatusDictionary = std::make_unique<QnResourceStatusDictionary>();
    d->resourcePropertyDictionary = std::make_unique<QnResourcePropertyDictionary>(this);
    d->cameraHistoryPool = std::make_unique<QnCameraHistoryPool>(this);
    d->serverAdditionalAddressesDictionary =
        std::make_unique<QnServerAdditionalAddressesDictionary>();
    d->runtimeInfoManager = std::make_unique<QnRuntimeInfoManager>();
    d->globalSettings = std::make_unique<SystemSettings>(this);
    d->userRolesManager = std::make_unique<QnUserRolesManager>(this);

    d->accessSubjectHierarchy = std::make_unique<ResourceAccessSubjectHierarchy>(
        d->resourcePool.get(),
        d->userRolesManager.get());

    d->globalPermissionsWatcher = std::make_unique<GlobalPermissionsWatcher>(
        d->resourcePool.get(),
        d->userRolesManager.get());

    d->accessRightsManager = std::make_unique<AccessRightsManager>();

    // Depends on resource pool and roles.
    d->sharedResourceManager = std::make_unique<QnSharedResourcesManager>(this);

    // Depends on resource pool and roles.
    d->globalPermissionsManager =
        std::make_unique<QnGlobalPermissionsManager>(resourceAccessMode, this);

    d->deprecatedAccessRightsConverter = std::make_unique<DeprecatedAccessRightsConverter>(
        d->resourcePool.get(),
        d->userRolesManager.get(),
        d->sharedResourceManager.get(),
        d->accessRightsManager.get());

    // Depends on resource pool, roles and global permissions.
    d->resourceAccessSubjectCache = std::make_unique<QnResourceAccessSubjectsCache>(this);

    // Depends on resource pool, roles and shared resources.
    d->resourceAccessProvider =
        std::make_unique<ResourceAccessProvider>(resourceAccessMode, this);

    // Some of base providers depend on QnGlobalPermissionsManager and QnSharedResourcesManager.
    d->resourceAccessProvider->addBaseProvider(
        new PermissionsResourceAccessProvider(resourceAccessMode, this));
    d->resourceAccessProvider->addBaseProvider(
        new SharedResourceAccessProvider(resourceAccessMode, this));
    d->resourceAccessProvider->addBaseProvider(
        new SharedLayoutItemAccessProvider(resourceAccessMode, this));
    d->resourceAccessProvider->addBaseProvider(
        new VideoWallItemAccessProvider(resourceAccessMode, this));
    d->resourceAccessProvider->addBaseProvider(
        new core::access::IntercomLayoutAccessProvider(resourceAccessMode, this));

    // Depends on access provider.
    d->resourceAccessManager = std::make_unique<QnResourceAccessManager>(this);

    d->showreelManager = std::make_unique<QnLayoutTourManager>();
    d->eventRuleManager = std::make_unique<nx::vms::event::RuleManager>();
    d->analyticsDescriptorContainer = std::make_unique<taxonomy::DescriptorContainer>(this);
    d->analyticsTaxonomyStateWatcher = std::make_unique<taxonomy::StateWatcher>(
        d->analyticsDescriptorContainer.get());

    switch (mode)
    {
        case Mode::default_:
            d->deviceLicenseUsageWatcher = std::make_unique<DeviceLicenseUsageWatcher>(this);
            d->videoWallLicenseUsageWatcher = std::make_unique<VideoWallLicenseUsageWatcher>(this);
            break;
        case Mode::unitTests:
            d->deviceLicenseUsageWatcher = std::make_unique<DeviceLicenseUsageWatcher>(this);
            d->videoWallLicenseUsageWatcher = std::make_unique<VideoWallLicenseUsageWatcher>(this);
            break;
    }
}

SystemContext::~SystemContext()
{
    d->resourcePool->threadPool()->waitForDone();
    d->resourceAccessProvider->clear();
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
    auto data = runtimeInfoManager()->localInfo();
    data.data.peer.instanceId = d->sessionId;
    runtimeInfoManager()->updateLocalItem(data);
}

void SystemContext::enableNetworking(AbstractCertificateVerifier* certificateVerifier)
{
    d->certificateVerifier = certificateVerifier;
}

AbstractCertificateVerifier* SystemContext::certificateVerifier() const
{
    return d->certificateVerifier;
}

void SystemContext::enableRouting(nx::vms::discovery::Manager* moduleDiscoveryManager)
{
    d->moduleDiscoveryManager = moduleDiscoveryManager;
}

bool SystemContext::isRoutingEnabled() const
{
    return !d->moduleDiscoveryManager.isNull();
}

nx::vms::discovery::Manager* SystemContext::moduleDiscoveryManager() const
{
    return d->moduleDiscoveryManager;
}

std::shared_ptr<ec2::AbstractECConnection> SystemContext::messageBusConnection() const
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

DeviceLicenseUsageWatcher* SystemContext::deviceLicenseUsageWatcher() const
{
    return d->deviceLicenseUsageWatcher.get();
}

VideoWallLicenseUsageWatcher* SystemContext::videoWallLicenseUsageWatcher() const
{
    return d->videoWallLicenseUsageWatcher.get();
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

SystemSettings* SystemContext::globalSettings() const
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

GlobalPermissionsWatcher* SystemContext::globalPermissionsWatcher() const
{
    return d->globalPermissionsWatcher.get();
}

AccessRightsManager* SystemContext::accessRightsManager() const
{
    return d->accessRightsManager.get();
}

DeprecatedAccessRightsConverter* SystemContext::deprecatedAccessRightsConverter() const
{
    return d->deprecatedAccessRightsConverter.get();
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

ResourceAccessSubjectHierarchy* SystemContext::accessSubjectHierarchy() const
{
    return d->accessSubjectHierarchy.get();
}

ResourceAccessProvider* SystemContext::resourceAccessProvider() const
{
    return d->resourceAccessProvider.get();
}

QnLayoutTourManager* SystemContext::showreelManager() const
{
    return d->showreelManager.get();
}

nx::vms::event::RuleManager* SystemContext::eventRuleManager() const
{
    return d->eventRuleManager.get();
}

nx::vms::rules::Engine* SystemContext::vmsRulesEngine() const
{
    return d->vmsRulesEngine;
}

void SystemContext::setVmsRulesEngine(nx::vms::rules::Engine* engine)
{
    d->vmsRulesEngine = engine;
}

taxonomy::DescriptorContainer* SystemContext::analyticsDescriptorContainer() const
{
    return d->analyticsDescriptorContainer.get();
}

taxonomy::AbstractStateWatcher* SystemContext::analyticsTaxonomyStateWatcher() const
{
    return d->analyticsTaxonomyStateWatcher.get();
}

std::shared_ptr<taxonomy::AbstractState> SystemContext::analyticsTaxonomyState() const
{
    if (d->analyticsTaxonomyStateWatcher)
        return d->analyticsTaxonomyStateWatcher->state();

    return nullptr;
}

SystemContext::Mode SystemContext::mode() const
{
    return d->mode;
}

void SystemContext::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    d->messageProcessor = messageProcessor;
    runtimeInfoManager()->setMessageProcessor(messageProcessor);
    cameraHistoryPool()->setMessageProcessor(messageProcessor);
}

} // namespace nx::vms::common
