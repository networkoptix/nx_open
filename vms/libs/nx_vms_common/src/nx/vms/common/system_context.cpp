// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtCore/QPointer>
#include <QtCore/QThreadPool>

#include <api/common_message_processor.h>
#include <nx/network/http/auth_tools.h>
#include <utils/common/long_runable_cleanup.h>

#include "private/system_context_data_p.h"

namespace nx::vms::common {

using namespace nx::analytics;
using namespace nx::core::access;

SystemContext::SystemContext(Mode mode, nx::Uuid peerId, QObject* parent):
    QObject(parent),
    d(new Private{.mode=mode, .peerId=peerId})
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
    d->saasServiceManager = std::make_unique<saas::ServiceManager>(this);
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

    d->metrics = std::make_shared<nx::metric::Storage>();
    d->lookupListManager = std::make_unique<LookupListManager>();
    d->pixelationSettings = std::make_unique<PixelationSettings>(this);

    // Some tests do not use network. It is OK to not initialize the pool for them.
    if (nx::network::SocketGlobals::isInitialized())
        d->httpClientPool = std::make_unique<network::http::ClientPool>();

    switch (d->mode)
    {
        case Mode::crossSystem:
        case Mode::cloudLayouts:
        case Mode::server:
            break;

        case Mode::client:
        case Mode::unitTests:
            d->deviceLicenseUsageWatcher = std::make_unique<DeviceLicenseUsageWatcher>(this);
            d->videoWallLicenseUsageWatcher = std::make_unique<VideoWallLicenseUsageWatcher>(this);
            break;
    }

    if (d->httpClientPool)
    {
        d->httpClientPool->setDefaultTimeouts(nx::network::http::AsyncClient::Timeouts{
            .sendTimeout = ::rest::kDefaultVmsApiTimeout,
            .responseReadTimeout = ::rest::kDefaultVmsApiTimeout,
            .messageBodyReadTimeout = ::rest::kDefaultVmsApiTimeout
        });
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
    // TODO: #sivanov Uncomment this code when destruction order will be fixed in both desktop and
    // mobile clients and a server (most probably when NetworkModule class will be cleaned up).
    //if (certificateVerifier)
    //{
    //    connect(certificateVerifier, &QObject::destroyed, this,
    //        [this]() { NX_ASSERT(false, "Invalid destruction order"); });
    //}
}

AbstractCertificateVerifier* SystemContext::verifier() const
{
    return d->certificateVerifier;
}

void SystemContext::startModuleDiscovery(nx::vms::discovery::Manager* moduleDiscoveryManager)
{
    d->moduleDiscoveryManager = moduleDiscoveryManager;
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

nx::network::http::Credentials SystemContext::credentials() const
{
    NX_ASSERT(false, "Credentials exist for the client-side contexts only");
    return {};
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

PixelationSettings* SystemContext::pixelationSettings() const
{
    return d->pixelationSettings.get();
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

nx::network::http::ClientPool* SystemContext::httpClientPool() const
{
    return d->httpClientPool.get();
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

//FIXME: #sivanov Old rules engine, should be cleaned up also.
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
    return analyticsTaxonomyStateWatcher()->state();
}

std::shared_ptr<nx::metric::Storage> SystemContext::metrics() const
{
    return d->metrics;
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

SystemContext::Mode SystemContext::mode() const
{
    return d->mode;
}

} // namespace nx::vms::common
