#include "client_module.h"

#include <memory>

#include <QtWidgets/QApplication>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/network_proxy_factory.h>
#include <api/runtime_info_manager.h>
#include <api/session_manager.h>

#include <common/common_module.h>
#ifdef Q_OS_WIN
    #include "common/systemexcept_win32.h"
#endif

#include <camera/camera_bookmarks_manager.h>

#include <client/client_app_info.h>
#include <client/client_startup_parameters.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_meta_types.h>
#include <client/client_translation_manager.h>
#include <client/client_instance_manager.h>
#include <client/desktop_client_message_processor.h>
#include <client/client_recent_connections_manager.h>

#include <core/core_settings.h>
#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource/client_camera_factory.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>

#include <platform/platform_abstraction.h>

#include <redass/redass_controller.h>

#include <server/server_storage_manager.h>

#include <ui/style/globals.h>

#include <utils/common/app_info.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/synctime.h>
#include <utils/media/ffmpeg_initializer.h>
#include <utils/media/voice_spectrum_analyzer.h>

#include <statistics/statistics_manager.h>
#include <statistics/storage/statistics_file_storage.h>
#include <statistics/settings/statistics_settings_watcher.h>
#include <ui/statistics/modules/controls_statistics_module.h>

#include <watchers/cloud_status_watcher.h>

namespace
{
    typedef std::unique_ptr<QnClientTranslationManager> QnClientTranslationManagerPtr;

    QnClientTranslationManagerPtr initializeTranslations(QnClientSettings *settings
        , const QString &dynamicTranslationPath)
    {
        QnClientTranslationManagerPtr translationManager(new QnClientTranslationManager());

        QnTranslation translation;
        if(!dynamicTranslationPath.isEmpty()) /* From command line. */
            translation = translationManager->loadTranslation(dynamicTranslationPath);

        if(translation.isEmpty()) /* By path. */
            translation = translationManager->loadTranslation(settings->translationPath());

        /* Check if qnSettings value is invalid. */
        if (translation.isEmpty())
            translation = translationManager->defaultTranslation();

        translationManager->installTranslation(translation);
        return std::move(translationManager);
    }

    void initializeStatisticsManager(QnCommonModule *commonModule)
    {
        const auto statManager = commonModule->instance<QnStatisticsManager>();

        statManager->setClientId(qnSettings->pcUuid());
        statManager->setStorage(QnStatisticsStoragePtr(new QnStatisticsFileStorage()));
        statManager->setSettings(QnStatisticsSettingsPtr(new QnStatisticsSettingsWatcher()));

        static const QScopedPointer<QnControlsStatisticsModule> controlsStatisticsModule(
            new QnControlsStatisticsModule());

        statManager->registerStatisticsModule(lit("controls"), controlsStatisticsModule.data());

        QObject::connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionClosed
            , statManager, &QnStatisticsManager::saveCurrentStatistics);
        QObject::connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionOpened
            , statManager, &QnStatisticsManager::resetStatistics);
        QObject::connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::initialResourcesReceived
            , statManager, &QnStatisticsManager::sendStatistics);

    }
}

QnClientModule::QnClientModule(const QnStartupParameters &startupParams
    , QObject *parent)
    : QObject(parent)
{
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(appserver2);
    QnClientMetaTypes::initialize();

    /* Set up application parameters so that QSettings know where to look for settings. */
    QApplication::setOrganizationName(QnAppInfo::organizationName());
    QApplication::setApplicationName(QnClientAppInfo::applicationName());
    QApplication::setApplicationDisplayName(QnClientAppInfo::applicationDisplayName());
    if (QApplication::applicationVersion().isEmpty())
        QApplication::setApplicationVersion(QnAppInfo::applicationVersion());
    QApplication::setStartDragDistance(20);

    /* We don't want changes in desktop color settings to mess up our custom style. */
    QApplication::setDesktopSettingsAware(false);

    typedef QScopedPointer<QnClientSettings> QnClientSettingsPtr;
    QnClientSettingsPtr clientSettings(new QnClientSettings(startupParams.forceLocalSettings));

    /// We should load translations before major client's services are started to prevent races
    QnClientTranslationManagerPtr translationManager(initializeTranslations(
        clientSettings.data(),  startupParams.dynamicTranslationPath));

    /* Init singletons. */

    QnCommonModule *common = new QnCommonModule(this);

    common->store<QnFfmpegInitializer>(new QnFfmpegInitializer());
    common->store<QnTranslationManager>(translationManager.release());
    common->store<QnCoreSettings>(new QnCoreSettings());
    common->store<QnClientRuntimeSettings>(new QnClientRuntimeSettings());
    common->store<QnClientSettings>(clientSettings.take());

    auto clientInstanceManager = new QnClientInstanceManager(); /* Depends on QnClientSettings */
    common->store<QnClientInstanceManager>(clientInstanceManager);
    common->setModuleGUID(clientInstanceManager->instanceGuid());

    common->store<QnGlobals>(new QnGlobals());
    common->store<QnSessionManager>(new QnSessionManager());

    common->store<QnRedAssController>(new QnRedAssController());

    common->store<QnPlatformAbstraction>(new QnPlatformAbstraction());

    common->store<QnClientPtzControllerPool>(new QnClientPtzControllerPool());
    common->store<QnDesktopClientMessageProcessor>(new QnDesktopClientMessageProcessor());
    common->store<QnCameraHistoryPool>(new QnCameraHistoryPool());
    common->store<QnRuntimeInfoManager>(new QnRuntimeInfoManager());
    common->store<QnClientResourceFactory>(new QnClientResourceFactory());

    common->store<QnResourcesChangesManager>(new QnResourcesChangesManager());
    common->store<QnCameraBookmarksManager>(new QnCameraBookmarksManager());
    common->store<QnServerStorageManager>(new QnServerStorageManager());
    common->store<QnClientRecentConnectionsManager>(new QnClientRecentConnectionsManager());

    common->store<QnVoiceSpectrumAnalyzer>(new QnVoiceSpectrumAnalyzer());

    initializeStatisticsManager(common);

    /* Long runnables depend on QnCameraHistoryPool and other singletons. */
    common->store<QnLongRunnablePool>(new QnLongRunnablePool());

    QnCloudStatusWatcher *cloudStatusWatcher = new QnCloudStatusWatcher();
    cloudStatusWatcher->setCloudEndpoint(qnSettings->cdbEndpoint());
    cloudStatusWatcher->setCloudCredentials(qnSettings->cloudLogin(), qnSettings->cloudPassword(), true);
    common->store<QnCloudStatusWatcher>(cloudStatusWatcher);

#ifdef Q_OS_WIN
    win32_exception::setCreateFullCrashDump(qnSettings->createFullCrashDump());
#endif

    //NOTE QNetworkProxyFactory::setApplicationProxyFactory takes ownership of object
    QNetworkProxyFactory::setApplicationProxyFactory(new QnNetworkProxyFactory());

    QnAppServerConnectionFactory::setDefaultFactory(QnClientResourceFactory::instance());
}

QnClientModule::~QnClientModule()
{
    QNetworkProxyFactory::setApplicationProxyFactory(nullptr);

    QApplication::setOrganizationName(QString());
    QApplication::setApplicationName(QString());
    QApplication::setApplicationDisplayName(QString());
    QApplication::setApplicationVersion(QString());
}
