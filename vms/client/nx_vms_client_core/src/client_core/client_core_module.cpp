// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_core_module.h"

#include <QtCore/QStandardPaths>
#include <QtQml/QQmlEngine>

#include <api/common_message_processor.h>
#include <client_core/client_core_meta_types.h>
#include <client_core/client_core_settings.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/dataprovider/data_provider_factory.h>
#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource_management/layout_tour_state_manager.h>
#include <core/resource_management/resources_changes_manager.h>
#include <nx/branding_proxy.h>
#include <nx/build_info_proxy.h>
#include <nx/core/access/access_types.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/local_network_interfaces_manager.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/session_token_terminator.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/thumbnails/thumbnail_image_provider.h>
#include <nx/vms/client/core/utils/operation_manager.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/rules/engine_holder.h>
#include <nx/vms/rules/initializer.h>
#include <plugins/resource/desktop_audio_only/desktop_audio_only_resource.h>
#include <watchers/cloud_status_watcher.h>

using namespace nx::vms::client::core;

template<> QnClientCoreModule* Singleton<QnClientCoreModule>::s_instance = nullptr;

struct QnClientCoreModule::Private
{
    std::unique_ptr<QnCommonModule> commonModule;
    std::unique_ptr<NetworkModule> networkModule;
    std::unique_ptr<QnDataProviderFactory> resourceDataProviderFactory;
    std::unique_ptr<QnCloudStatusWatcher> cloudStatusWatcher;
    std::unique_ptr<nx::vms::rules::EngineHolder> vmsRulesEngineHolder;
    std::unique_ptr<SessionTokenTerminator> sessionTokenTerminator;
    QQmlEngine* qmlEngine = nullptr;
};

//-------------------------------------------------------------------------------------------------
// QnClientCoreModule

QnClientCoreModule::QnClientCoreModule(
    Mode mode,
    SystemContext* systemContext)
    :
    base_type(),
    d(new Private())
{
    NX_ASSERT(!systemContext->peerId().isNull()
        || mode == Mode::unitTests, "Production usage must have peer id.");

    d->commonModule = std::make_unique<QnCommonModule>(
        /*clientMode*/ true,
        systemContext);

    ini().reload();

    Q_INIT_RESOURCE(nx_vms_client_core);
    initializeMetaTypes();

    d->commonModule->store(new QnClientCoreSettings());

    // Disable secure settings in unit tests.
    const bool useKeyChain = (mode != Mode::unitTests);
    d->commonModule->store(new Settings(useKeyChain));

    d->commonModule->instance<QnResourcesChangesManager>();
    d->commonModule->instance<QnClientPtzControllerPool>();
    d->commonModule->instance<QnLayoutTourStateManager>();

    d->commonModule->store(new nx::vms::client::core::LocalNetworkInterfacesManager());
    d->commonModule->store(new watchers::KnownServerConnections(d->commonModule.get()));
    d->commonModule->store(new OperationManager());

    auto brandingQmlProxy = new nx::branding::QmlProxy(this);
    qmlRegisterSingletonType<nx::branding::QmlProxy>("nx.vms.client.core", 1, 0, "Branding",
        [brandingQmlProxy](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            qmlEngine->setObjectOwnership(brandingQmlProxy, QQmlEngine::CppOwnership);
            return brandingQmlProxy;
        });

    auto buildInfoQmlProxy = new nx::build_info::QmlProxy(this);
    qmlRegisterSingletonType<nx::build_info::QmlProxy>("nx.vms.client.core", 1, 0, "BuildInfo",
        [buildInfoQmlProxy](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            qmlEngine->setObjectOwnership(buildInfoQmlProxy, QQmlEngine::CppOwnership);
            return buildInfoQmlProxy;
        });

    d->resourceDataProviderFactory.reset(new QnDataProviderFactory());

    d->qmlEngine = new QQmlEngine(this);
    d->qmlEngine->setOutputWarningsToStandardError(true);
    systemContext->storeToQmlContext(d->qmlEngine->rootContext());

    const auto thumbnailProvider = new ThumbnailImageProvider();
    // QQmlEngine takes ownership of thumbnailProvider.
    d->qmlEngine->addImageProvider(ThumbnailImageProvider::id, thumbnailProvider);

    d->resourceDataProviderFactory->registerResourceType<QnDesktopAudioOnlyResource>();
    d->cloudStatusWatcher = std::make_unique<QnCloudStatusWatcher>(d->commonModule.get());

    d->vmsRulesEngineHolder = std::make_unique<nx::vms::rules::EngineHolder>(
        systemContext,
        std::make_unique<nx::vms::rules::Initializer>(systemContext));
    d->sessionTokenTerminator = std::make_unique<SessionTokenTerminator>();
}

QnClientCoreModule::~QnClientCoreModule()
{
}

void QnClientCoreModule::initializeNetworking(
    nx::vms::api::PeerType peerType,
    Qn::SerializationFormat serializationFormat,
    CertificateValidationLevel certificateValidationLevel)
{
    d->networkModule = std::make_unique<NetworkModule>(
        d->commonModule.get(),
        peerType,
        serializationFormat,
        certificateValidationLevel);

    connect(&settings()->certificateValidationLevel, &Settings::BaseProperty::changed, this,
        [this](nx::utils::property_storage::BaseProperty* /*property*/)
        {
            const auto level = settings()->certificateValidationLevel();
            d->networkModule->reinitializeCertificateStorage(level);
        });
}

NetworkModule* QnClientCoreModule::networkModule() const
{
    return d->networkModule.get();
}

QnResourcePool* QnClientCoreModule::resourcePool() const
{
    return d->commonModule->systemContext()->resourcePool();
}

QnCommonModule* QnClientCoreModule::commonModule() const
{
    return d->commonModule.get();
}

QnPtzControllerPool* QnClientCoreModule::ptzControllerPool() const
{
    return d->commonModule->instance<QnClientPtzControllerPool>();
}

QnLayoutTourStateManager* QnClientCoreModule::layoutTourStateManager() const
{
    return d->commonModule->instance<QnLayoutTourStateManager>();
}

QnDataProviderFactory* QnClientCoreModule::dataProviderFactory() const
{
    return d->resourceDataProviderFactory.get();
}

QQmlEngine* QnClientCoreModule::mainQmlEngine()
{
    return d->qmlEngine;
}

QnCloudStatusWatcher* QnClientCoreModule::cloudStatusWatcher() const
{
    return d->cloudStatusWatcher.get();
}

nx::vms::rules::Engine* QnClientCoreModule::vmsRulesEngine() const
{
    return d->vmsRulesEngineHolder->engine();
}
SessionTokenTerminator* QnClientCoreModule::sessionTokenTerminator() const
{
    return d->sessionTokenTerminator.get();
}
