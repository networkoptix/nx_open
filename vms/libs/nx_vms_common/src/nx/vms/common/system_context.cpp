// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtCore/QPointer>
#include <QtCore/QThreadPool>

#include <api/common_message_processor.h>
#include <api/runtime_info_manager.h>
#include <core/resource/camera_history.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/status_dictionary.h>
#include <licensing/license.h>
#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/analytics/taxonomy/state_watcher.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/vms/common/license/license_usage_watcher.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/event/rule_manager.h>
#include <utils/camera/camera_names_watcher.h>

namespace nx::vms::common {

using namespace nx::analytics;
using namespace nx::core::access;

struct SystemContext::Private
{
    const Mode mode;
    const nx::Uuid peerId;
    QnCommonMessageProcessor* messageProcessor = nullptr;

    std::unique_ptr<QnLicensePool> licensePool;
    std::unique_ptr<saas::ServiceManager> saasServiceManager;
    std::unique_ptr<QnResourcePool> resourcePool;
    std::unique_ptr<QnResourceDataPool> resourceDataPool;
    std::unique_ptr<QnResourceStatusDictionary> resourceStatusDictionary;
    std::unique_ptr<QnResourcePropertyDictionary> resourcePropertyDictionary;
    std::unique_ptr<QnCameraHistoryPool> cameraHistoryPool;
    std::unique_ptr<QnServerAdditionalAddressesDictionary> serverAdditionalAddressesDictionary;
    std::unique_ptr<QnRuntimeInfoManager> runtimeInfoManager;
    std::unique_ptr<SystemSettings> globalSettings;
    std::unique_ptr<UserGroupManager> userGroupManager;
    std::unique_ptr<ResourceAccessSubjectHierarchy> accessSubjectHierarchy;
    std::unique_ptr<GlobalPermissionsWatcher> globalPermissionsWatcher;
    std::unique_ptr<AccessRightsManager> accessRightsManager;
    std::unique_ptr<QnResourceAccessManager> resourceAccessManager;
    std::unique_ptr<ShowreelManager> showreelManager;
    std::unique_ptr<LookupListManager> lookupListManager;
    std::unique_ptr<nx::vms::event::RuleManager> eventRuleManager;
    std::unique_ptr<taxonomy::DescriptorContainer> analyticsDescriptorContainer;
    std::unique_ptr<taxonomy::AbstractStateWatcher> analyticsTaxonomyStateWatcher;
    std::shared_ptr<nx::metrics::Storage> metrics;
    std::unique_ptr<DeviceLicenseUsageWatcher> deviceLicenseUsageWatcher;
    std::unique_ptr<VideoWallLicenseUsageWatcher> videoWallLicenseUsageWatcher;

    QPointer<AbstractCertificateVerifier> certificateVerifier;
    QPointer<nx::vms::discovery::Manager> moduleDiscoveryManager;
    nx::vms::rules::Engine* vmsRulesEngine = {};
    std::unique_ptr<QnCameraNamesWatcher> cameraNamesWatcher;
};

SystemContext::SystemContext(
    Mode mode,
    nx::Uuid peerId,
    nx::core::access::Mode resourceAccessMode,
    QObject* parent)
    :
    QObject(parent),
    d(new Private{
        .mode = mode,
        .peerId = std::move(peerId)
    })
{
    d->licensePool = std::make_unique<QnLicensePool>(this);
    d->resourcePool = std::make_unique<QnResourcePool>(this);
    d->resourceDataPool = std::make_unique<QnResourceDataPool>();
    d->resourceStatusDictionary = std::make_unique<QnResourceStatusDictionary>();
    d->resourcePropertyDictionary = std::make_unique<QnResourcePropertyDictionary>(this);
    d->saasServiceManager = std::make_unique<saas::ServiceManager>(this);
    d->cameraHistoryPool = std::make_unique<QnCameraHistoryPool>(this);
    d->serverAdditionalAddressesDictionary =
        std::make_unique<QnServerAdditionalAddressesDictionary>();
    d->runtimeInfoManager = std::make_unique<QnRuntimeInfoManager>();
    d->globalSettings = std::make_unique<SystemSettings>(this);

    d->userGroupManager = std::make_unique<UserGroupManager>();

    d->accessSubjectHierarchy = std::make_unique<ResourceAccessSubjectHierarchy>(
        d->resourcePool.get(),
        d->userGroupManager.get());

    d->globalPermissionsWatcher = std::make_unique<GlobalPermissionsWatcher>(
        d->resourcePool.get(),
        d->userGroupManager.get());

    d->accessRightsManager = std::make_unique<AccessRightsManager>();

    // Depends on access provider.
    d->resourceAccessManager = std::make_unique<QnResourceAccessManager>(this);

    d->showreelManager = std::make_unique<ShowreelManager>();
    d->eventRuleManager = std::make_unique<nx::vms::event::RuleManager>();
    d->analyticsDescriptorContainer = std::make_unique<taxonomy::DescriptorContainer>(this);
    d->analyticsTaxonomyStateWatcher = std::make_unique<taxonomy::StateWatcher>(
        d->analyticsDescriptorContainer.get(),
        this);

    d->metrics = std::make_shared<nx::metrics::Storage>();
    switch (mode)
    {
        case Mode::server:
            d->lookupListManager = std::make_unique<LookupListManager>();
            break;
        case Mode::client:
            d->lookupListManager = std::make_unique<LookupListManager>();
            d->deviceLicenseUsageWatcher = std::make_unique<DeviceLicenseUsageWatcher>(this);
            d->videoWallLicenseUsageWatcher = std::make_unique<VideoWallLicenseUsageWatcher>(this);
            break;
        case Mode::unitTests:
            d->lookupListManager = std::make_unique<LookupListManager>();
            d->deviceLicenseUsageWatcher = std::make_unique<DeviceLicenseUsageWatcher>(this);
            d->videoWallLicenseUsageWatcher = std::make_unique<VideoWallLicenseUsageWatcher>(this);
            break;
    }

    d->cameraNamesWatcher = std::make_unique<QnCameraNamesWatcher>((this));
}

SystemContext::~SystemContext()
{
    d->resourcePool->threadPool()->waitForDone();
}

const nx::Uuid& SystemContext::peerId() const
{
    return d->peerId;
}

nx::Uuid SystemContext::auditId() const
{
    // TODO: #sivanov Remove the base method when QnRtspClientArchiveDelegate is fixed.
    NX_ASSERT(false, "Base class implementation has no Audit ID. Method must be overridden.");
    return {};
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

saas::ServiceManager* SystemContext::saasServiceManager() const
{
    return d->saasServiceManager.get();
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

UserGroupManager* SystemContext::userGroupManager() const
{
    return d->userGroupManager.get();
}

GlobalPermissionsWatcher* SystemContext::globalPermissionsWatcher() const
{
    return d->globalPermissionsWatcher.get();
}

AccessRightsManager* SystemContext::accessRightsManager() const
{
    return d->accessRightsManager.get();
}

QnResourceAccessManager* SystemContext::resourceAccessManager() const
{
    return d->resourceAccessManager.get();
}

ResourceAccessSubjectHierarchy* SystemContext::accessSubjectHierarchy() const
{
    return d->accessSubjectHierarchy.get();
}

ShowreelManager* SystemContext::showreelManager() const
{
    return d->showreelManager.get();
}

LookupListManager* SystemContext::lookupListManager() const
{
    return d->lookupListManager.get();
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

std::shared_ptr<nx::metrics::Storage> SystemContext::metrics() const
{
    return d->metrics;
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

QnCameraNamesWatcher* SystemContext::cameraNamesWatcher() const
{
    return d->cameraNamesWatcher.get();
}

} // namespace nx::vms::common
