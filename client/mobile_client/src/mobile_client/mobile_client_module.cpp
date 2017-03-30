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

using namespace nx::mobile_client;

QnMobileClientModule::QnMobileClientModule(
    const QnMobileClientStartupParameters& startupParameters,
    QObject* parent)
    :
    QObject(parent)
{
    Q_INIT_RESOURCE(appserver2);
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
    m_commonModule = new QnCommonModule(this);
    m_commonModule->setModuleGUID(QnUuid::createUuid());
    nx::network::SocketGlobals::outgoingTunnelPool().assignOwnPeerId("mc", m_commonModule->moduleGUID());

    // TODO: #mu ON/OFF switch in settings?
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    // TODO: #mshevchenko Remove when client_core_module is created.
    m_commonModule->store(new QnFfmpegInitializer());

    m_commonModule->store(translationManager);
    m_commonModule->store(new QnClientCoreSettings());
    m_commonModule->store(new QnMobileClientSettings);
    settings::migrateSettings();
    m_commonModule->store(new QnSessionManager());

    m_commonModule->store(new QnLongRunnablePool());
    m_commonModule->store(new QnMobileClientMessageProcessor());
    m_commonModule->store(new QnCameraHistoryPool());
    m_commonModule->store(new QnRuntimeInfoManager());
    m_commonModule->store(new QnMobileClientCameraFactory());

    m_commonModule->store(new QnResourcesChangesManager());

    auto userWatcher = m_commonModule->store(new QnUserWatcher());

    auto availableCamerasWatcher = m_commonModule->store(new QnAvailableCamerasWatcher());
    connect(userWatcher, &QnUserWatcher::userChanged,
        availableCamerasWatcher, &QnAvailableCamerasWatcher::setUser);

    m_commonModule->store(new QnCloudConnectionProvider());
    m_commonModule->store(new QnCloudStatusWatcher());
    QNetworkProxyFactory::setApplicationProxyFactory(new QnSimpleNetworkProxyFactory());

    QnAppServerConnectionFactory::setDefaultFactory(QnMobileClientCameraFactory::instance());

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = m_commonModule->moduleGUID();
    runtimeData.peer.instanceId = m_commonModule->runningInstanceGUID();
    runtimeData.peer.peerType = Qn::PT_MobileClient;
    runtimeData.peer.dataFormat = Qn::JsonFormat;
    runtimeData.brand = QnAppInfo::productNameShort();
    runtimeData.customization = QnAppInfo::customizationName();
    if (!startupParameters.videowallInstanceGuid.isNull())
        runtimeData.videoWallInstanceGuid = startupParameters.videowallInstanceGuid;
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);

    auto moduleFinder = m_commonModule->store(new QnModuleFinder(true));
    moduleFinder->multicastModuleFinder()->setCheckInterfacesTimeout(10 * 1000);
    moduleFinder->start();

    m_commonModule->store(new QnRouter(moduleFinder));

    const auto getter = []() { return qnClientCoreSettings->knownServerUrls(); };
    const auto setter =
        [](const QnServerAddressWatcher::UrlsList& values)
        {
            qnClientCoreSettings->setKnownServerUrls(values);
            qnClientCoreSettings->save();
        };
    m_commonModule->store(new QnServerAddressWatcher(getter, setter));

    m_commonModule->store(new QnSystemsFinder());
    m_commonModule->store(new QnSystemsWeightsManager());

    m_commonModule->store(new settings::SessionsMigrationHelper());

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

QnCommonModule* QnMobileClientModule::commonModule() const
{
    return m_commonModule;
}
