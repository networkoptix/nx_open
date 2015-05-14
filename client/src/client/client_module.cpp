#include "client_module.h"

#include <QtWidgets/QApplication>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/network_proxy_factory.h>
#include <api/runtime_info_manager.h>
#include <api/session_manager.h>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_meta_types.h>
#include <client/client_message_processor.h>

#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>

#include <platform/platform_abstraction.h>

#include <plugins/resource/server_camera/server_camera_factory.h>

#include <utils/common/app_info.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/synctime.h>

#include "version.h"

QnClientModule::QnClientModule(bool forceLocalSettings, QObject *parent): QObject(parent) {
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

    QnClientSettings *settings = new QnClientSettings(forceLocalSettings);
    common->store<QnClientSettings>(settings);
    common->store<QnSessionManager>(new QnSessionManager());

    common->setModuleGUID(QnUuid::createUuid());

    m_cameraUserAttributePool.reset(new QnCameraUserAttributePool());
    m_mediaServerUserAttributesPool.reset(new QnMediaServerUserAttributesPool());
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

    //NOTE QNetworkProxyFactory::setApplicationProxyFactory takes ownership of object
    QNetworkProxyFactory::setApplicationProxyFactory(new QnNetworkProxyFactory());

    QnAppServerConnectionFactory::setClientGuid(QnUuid::createUuid().toString());
    QnAppServerConnectionFactory::setDefaultFactory(QnServerCameraFactory::instance());
}

QnClientModule::~QnClientModule() {
    QNetworkProxyFactory::setApplicationProxyFactory(nullptr);

    QApplication::setOrganizationName(QString());
    QApplication::setApplicationName(QString());
    QApplication::setApplicationDisplayName(QString());
    QApplication::setApplicationVersion(QString());     
}

