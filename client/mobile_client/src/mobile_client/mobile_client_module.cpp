#include "mobile_client_module.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QDesktopServices>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/mobile_client_camera_factory.h>
#include <client_core/client_core_settings.h>
#include <api/app_server_connection.h>
#include <api/session_manager.h>
#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <api/simple_network_proxy_factory.h>
#include <utils/common/long_runnable.h>
#include <utils/common/app_info.h>
#include <network/module_finder.h>
#include <network/multicast_module_finder.h>
#include <network/router.h>
#include <cloud/cloud_connection.h>
#include <watchers/user_watcher.h>
#include <watchers/available_cameras_watcher.h>
#include <watchers/cloud_status_watcher.h>
#include <finders/systems_finder.h>
#include <client/client_recent_connections_manager.h>
#include <client/system_weights_manager.h>
#include <utils/media/ffmpeg_initializer.h>

#include "mobile_client_message_processor.h"
#include "mobile_client_meta_types.h"
#include "mobile_client_settings.h"
#include "mobile_client_translation_manager.h"
#include "mobile_client_app_info.h"
#include "mobile_client_startup_parameters.h"
#include <nx/network/socket_global.h>


QnMobileClientModule::QnMobileClientModule(
    const QnMobileClientStartupParameters& startupParameters,
    QObject* parent)
    :
    QObject(parent)
{
    Q_INIT_RESOURCE(mobile_client);
    Q_INIT_RESOURCE(appserver2);

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
    QnCommonModule *common = new QnCommonModule(this);
    common->setModuleGUID(QnUuid::createUuid());
    nx::network::SocketGlobals::outgoingTunnelPool().designateOwnPeerId("mc", common->moduleGUID());

    // TODO: #mshevchenko Remove when client_core_module is created.
    common->store<QnFfmpegInitializer>(new QnFfmpegInitializer());

    common->store<QnTranslationManager>(translationManager);
    common->store<QnClientCoreSettings>(new QnClientCoreSettings());
    common->store<QnMobileClientSettings>(new QnMobileClientSettings);
    common->store<QnSessionManager>(new QnSessionManager());

    common->store<QnLongRunnablePool>(new QnLongRunnablePool());
    common->store<QnMobileClientMessageProcessor>(new QnMobileClientMessageProcessor());
    common->store<QnCameraHistoryPool>(new QnCameraHistoryPool());
    common->store<QnRuntimeInfoManager>(new QnRuntimeInfoManager());
    common->store<QnMobileClientCameraFactory>(new QnMobileClientCameraFactory());
    common->store<QnClientRecentConnectionsManager>(new QnClientRecentConnectionsManager());

    common->store<QnResourcesChangesManager>(new QnResourcesChangesManager());

    QnUserWatcher *userWatcher = new QnUserWatcher();
    common->store<QnUserWatcher>(userWatcher);

    QnAvailableCamerasWatcher *availableCamerasWatcher = new QnAvailableCamerasWatcher();
    common->store<QnAvailableCamerasWatcher>(availableCamerasWatcher);
    connect(userWatcher, &QnUserWatcher::userChanged, availableCamerasWatcher, &QnAvailableCamerasWatcher::setUser);

    common->store<QnCloudConnectionProvider>(new QnCloudConnectionProvider());
    common->store<QnCloudStatusWatcher>(new QnCloudStatusWatcher());
    QNetworkProxyFactory::setApplicationProxyFactory(new QnSimpleNetworkProxyFactory());

    QnAppServerConnectionFactory::setDefaultFactory(QnMobileClientCameraFactory::instance());

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = Qn::PT_MobileClient;
    runtimeData.peer.dataFormat = Qn::JsonFormat;
    runtimeData.brand = QnAppInfo::productNameShort();
    runtimeData.customization = QnAppInfo::customizationName();
    if (!startupParameters.videowallInstanceGuid.isNull())
        runtimeData.videoWallInstanceGuid = startupParameters.videowallInstanceGuid;
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);

    auto moduleFinder = new QnModuleFinder(true);
    common->store<QnModuleFinder>(moduleFinder);
    moduleFinder->multicastModuleFinder()->setCheckInterfacesTimeout(10 * 1000);
    moduleFinder->start();

    common->store<QnRouter>(new QnRouter(moduleFinder));

    common->store<QnSystemsFinder>(new QnSystemsFinder());
    common->store<QnSystemsWeightsManager>(new QnSystemsWeightsManager());

    connect(qApp, &QGuiApplication::applicationStateChanged, this,
        [moduleFinder](Qt::ApplicationState state)
        {
            switch (state)
            {
                case Qt::ApplicationActive:
                    moduleFinder->start();
                    break;
                case Qt::ApplicationSuspended:
                    moduleFinder->pleaseStop();
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
