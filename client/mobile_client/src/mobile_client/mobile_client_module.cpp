#include "mobile_client_module.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QDesktopServices>

#include <api/app_server_connection.h>

#include <common/common_module.h>
#include <common/static_common_module.h>


#include <client_core/client_core_module.h>


#include <core/resource_management/resource_pool.h>
#include <core/resource/mobile_client_camera_factory.h>
#include <api/session_manager.h>
#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <api/simple_network_proxy_factory.h>
#include <nx/utils/thread/long_runnable.h>
#include <utils/common/app_info.h>
#include <nx/vms/discovery/manager.h>
#include <network/router.h>
#include <cloud/cloud_connection.h>
#include <watchers/user_watcher.h>
#include <watchers/available_cameras_watcher.h>
#include <watchers/cloud_status_watcher.h>
#include <watchers/server_address_watcher.h>
#include <finders/systems_finder.h>
#include <client/system_weights_manager.h>
#include <utils/media/ffmpeg_initializer.h>

#include "mobile_client_message_processor.h"
#include "mobile_client_meta_types.h"
#include "mobile_client_settings.h"
#include "mobile_client_translation_manager.h"
#include "mobile_client_app_info.h"
#include "mobile_client_startup_parameters.h"
#include <nx/network/socket_global.h>
#include <nx/mobile_client/settings/migration_helper.h>
#include <nx/mobile_client/settings/settings_migration.h>
#include <client_core/client_core_settings.h>
#include <core/ptz/client_ptz_controller_pool.h>

using namespace nx::mobile_client;

QnMobileClientModule::QnMobileClientModule(
    const QnMobileClientStartupParameters& startupParameters,
    QObject* parent)
    :
    QObject(parent)
{
    Q_INIT_RESOURCE(mobile_client);

    QnMobileClientMetaTypes::initialize();

    /* Set up application parameters so that QSettings know where to look for settings. */
    QGuiApplication::setOrganizationName(QnAppInfo::organizationName());
    QGuiApplication::setApplicationName(QnMobileClientAppInfo::applicationName());
    QGuiApplication::setApplicationDisplayName(QnMobileClientAppInfo::applicationDisplayName());
    QGuiApplication::setApplicationVersion(QnAppInfo::applicationVersion());

    // We should load translations before major client's services are started to prevent races
    QnMobileClientTranslationManager *translationManager = new QnMobileClientTranslationManager();
    translationManager->updateTranslation();

    /* Init singletons. */
    m_clientCoreModule = new QnClientCoreModule(this);

    auto commonModule = m_clientCoreModule->commonModule();
    commonModule->setModuleGUID(QnUuid::createUuid());
    nx::network::SocketGlobals::outgoingTunnelPool().assignOwnPeerId("mc", commonModule->moduleGUID());

    // TODO: #mu ON/OFF switch in settings?
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    // TODO: #mshevchenko Remove when client_core_module is created.
    commonModule->store(translationManager);

    commonModule->store(new QnMobileClientSettings);
    settings::migrateSettings();

    commonModule->store(new QnLongRunnablePool());
    commonModule->createMessageProcessor<QnMobileClientMessageProcessor>();

    commonModule->instance<QnCameraHistoryPool>();
    commonModule->store(new QnMobileClientCameraFactory());

    auto userWatcher = commonModule->store(new QnUserWatcher());

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = commonModule->moduleGUID();
    runtimeData.peer.instanceId = commonModule->runningInstanceGUID();
    runtimeData.peer.peerType = qnStaticCommon->localPeerType();
    runtimeData.peer.dataFormat = Qn::JsonFormat;
    runtimeData.brand = qnStaticCommon->brand();
    runtimeData.customization = QnAppInfo::customizationName();
    if (!startupParameters.videowallInstanceGuid.isNull())
        runtimeData.videoWallInstanceGuid = startupParameters.videowallInstanceGuid;
    commonModule->runtimeInfoManager()->updateLocalItem(runtimeData);

    auto availableCamerasWatcher = commonModule->instance<QnAvailableCamerasWatcher>();
    connect(userWatcher, &QnUserWatcher::userChanged,
        availableCamerasWatcher, &QnAvailableCamerasWatcher::setUser);

    commonModule->store(new QnCloudConnectionProvider());
    commonModule->instance<QnCloudStatusWatcher>();
    QNetworkProxyFactory::setApplicationProxyFactory(new QnSimpleNetworkProxyFactory(commonModule));

    const auto getter = []() { return qnClientCoreSettings->knownServerUrls(); };
    const auto setter =
        [](const QnServerAddressWatcher::UrlsList& values)
        {
            qnClientCoreSettings->setKnownServerUrls(values);
            qnClientCoreSettings->save();
        };
    commonModule->store(new QnServerAddressWatcher(getter, setter, commonModule));

    commonModule->instance<QnSystemsFinder>();
    commonModule->store(new QnSystemsWeightsManager());

    commonModule->store(new settings::SessionsMigrationHelper());

    connect(qApp, &QGuiApplication::applicationStateChanged, this,
        [moduleManager = commonModule->moduleDiscoveryManager()](Qt::ApplicationState state)
        {
            switch (state)
            {
                case Qt::ApplicationActive:
                    moduleManager->start();
                    break;
                case Qt::ApplicationSuspended:
                    moduleManager->stop();
                    break;
                default:
                    break;
            }
        });
}

QnMobileClientModule::~QnMobileClientModule()
{
    qApp->disconnect(this);
    QNetworkProxyFactory::setApplicationProxyFactory(nullptr);
}
