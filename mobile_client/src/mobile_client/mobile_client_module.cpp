#include "mobile_client_module.h"

#include <QtGui/QGuiApplication>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
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
#include <watchers/user_watcher.h>
#include <watchers/available_cameras_watcher.h>
#include <watchers/cloud_status_watcher.h>
#include <finders/systems_finder.h>
#include <finders/cloud_systems_finder.h>
#include <finders/direct_systems_finder.h>
#include <client/client_recent_connections_manager.h>

#include "mobile_client_message_processor.h"
#include "mobile_client_meta_types.h"
#include "mobile_client_settings.h"
#include "mobile_client_translation_manager.h"
#include "mobile_client_app_info.h"

QnMobileClientModule::QnMobileClientModule(QObject *parent) :
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

    QnUserWatcher *userWatcher = new QnUserWatcher();
    common->store<QnUserWatcher>(userWatcher);

    QnAvailableCamerasWatcher *availableCamerasWatcher = new QnAvailableCamerasWatcher();
    common->store<QnAvailableCamerasWatcher>(availableCamerasWatcher);
    connect(userWatcher, &QnUserWatcher::userChanged, availableCamerasWatcher, &QnAvailableCamerasWatcher::setUser);

    auto cloudStatusWatcher = new QnCloudStatusWatcher();
    cloudStatusWatcher->setCloudEndpoint(qnClientCoreSettings->cdbEndpoint());
    cloudStatusWatcher->setCloudCredentials(qnClientCoreSettings->cloudLogin(),
                                            qnClientCoreSettings->cloudPassword(),
                                            true);
    common->store<QnCloudStatusWatcher>(cloudStatusWatcher);

    QNetworkProxyFactory::setApplicationProxyFactory(new QnSimpleNetworkProxyFactory());

    QnAppServerConnectionFactory::setDefaultFactory(QnMobileClientCameraFactory::instance());

    QnModuleFinder *moduleFinder = new QnModuleFinder(true, false);
    common->store<QnModuleFinder>(moduleFinder);
    moduleFinder->multicastModuleFinder()->setCheckInterfacesTimeout(10 * 1000);
    moduleFinder->start();

    common->store<QnRouter>(new QnRouter(moduleFinder));

    QnSystemsFinder* systemsFinder(new QnSystemsFinder());
    systemsFinder->addSystemsFinder(new QnDirectSystemsFinder(systemsFinder));
    systemsFinder->addSystemsFinder(new QnCloudSystemsFinder(systemsFinder));
    common->store<QnSystemsFinder>(systemsFinder);

    connect(qApp, &QGuiApplication::applicationStateChanged, this, [moduleFinder](Qt::ApplicationState state) {
        switch (state) {
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

QnMobileClientModule::~QnMobileClientModule() {
    disconnect(qApp, nullptr, this, nullptr);
    QNetworkProxyFactory::setApplicationProxyFactory(nullptr);
}
