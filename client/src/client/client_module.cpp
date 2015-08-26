#include "client_module.h"

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

#include <client/client_startup_parameters.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_meta_types.h>
#include <client/client_translation_manager.h>

#include <redass/redass_controller.h>
#include <client/client_message_processor.h>
#include <client/client_instance_manager.h>

#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>

#include <platform/platform_abstraction.h>

#include <plugins/resource/server_camera/server_camera_factory.h>

#include <ui/style/globals.h>

#include <utils/common/app_info.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/synctime.h>

#include "version.h"

namespace
{
    void initializeTranslations(QnCommonModule *common
        , const QString &translationPath
        , bool forceLocalSettings)
    {
        const auto translationManager = common->instance<QnClientTranslationManager>();

        QnTranslation translation;
        if(!translationPath.isEmpty()) /* From command line. */
            translation = translationManager->loadTranslation(translationPath);

        if(translation.isEmpty()) /* By path. */
        {
            typedef QScopedPointer<QnClientSettings> ClientSettings;
            const ClientSettings settings(new QnClientSettings(forceLocalSettings));
            translation = translationManager->loadTranslation(settings->translationPath());
        }

        /* Check if qnSettings value is invalid. */
        if (translation.isEmpty()) 
            translation = translationManager->defaultTranslation();

        translationManager->installTranslation(translation);
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
    QApplication::setApplicationName(lit(QN_APPLICATION_NAME));
    QApplication::setApplicationDisplayName(lit(QN_APPLICATION_DISPLAY_NAME));    
    if (QApplication::applicationVersion().isEmpty())
        QApplication::setApplicationVersion(QnAppInfo::applicationVersion());
    QApplication::setStartDragDistance(20);
 
    /* We don't want changes in desktop color settings to mess up our custom style. */
    QApplication::setDesktopSettingsAware(false);
 
    /* Init singletons. */
    QnCommonModule *common = new QnCommonModule(this);

    /// We should load translations before major client's services are started to prevent races
    initializeTranslations(qnCommon, startupParams.translationPath, startupParams.forceLocalSettings);

    common->store<QnClientRuntimeSettings>(new QnClientRuntimeSettings());
    common->store<QnClientSettings>(new QnClientSettings(startupParams.forceLocalSettings));

    auto clientInstanceManager = new QnClientInstanceManager(); /* Depends on QnClientSettings */
    common->store<QnClientInstanceManager>(clientInstanceManager); 
    common->setModuleGUID(clientInstanceManager->instanceGuid());

    common->store<QnGlobals>(new QnGlobals());
    common->store<QnSessionManager>(new QnSessionManager());

    common->store<QnCameraUserAttributePool>(new QnCameraUserAttributePool());
    common->store<QnMediaServerUserAttributesPool>(new QnMediaServerUserAttributesPool());
    common->store<QnRedAssController>(new QnRedAssController());

    common->store<QnResourcePool>(new QnResourcePool());
    common->store<QnSyncTime>(new QnSyncTime());

    common->store<QnResourcePropertyDictionary>(new QnResourcePropertyDictionary());
    common->store<QnResourceStatusDictionary>(new QnResourceStatusDictionary());
    common->store<QnServerAdditionalAddressesDictionary>(new QnServerAdditionalAddressesDictionary());

    common->store<QnPlatformAbstraction>(new QnPlatformAbstraction());
    common->store<QnLongRunnablePool>(new QnLongRunnablePool());
    common->store<QnClientPtzControllerPool>(new QnClientPtzControllerPool());
    common->store<QnGlobalSettings>(new QnGlobalSettings());
    common->store<QnClientMessageProcessor>(new QnClientMessageProcessor());
    common->store<QnRuntimeInfoManager>(new QnRuntimeInfoManager());
    common->store<QnServerCameraFactory>(new QnServerCameraFactory());

#ifdef Q_OS_WIN
    win32_exception::setCreateFullCrashDump(qnSettings->createFullCrashDump());
#endif

    //NOTE QNetworkProxyFactory::setApplicationProxyFactory takes ownership of object
    QNetworkProxyFactory::setApplicationProxyFactory(new QnNetworkProxyFactory());

    QnAppServerConnectionFactory::setDefaultFactory(QnServerCameraFactory::instance());
}

QnClientModule::~QnClientModule() {
    QNetworkProxyFactory::setApplicationProxyFactory(nullptr);

    QApplication::setOrganizationName(QString());
    QApplication::setApplicationName(QString());
    QApplication::setApplicationDisplayName(QString());
    QApplication::setApplicationVersion(QString());     
}

