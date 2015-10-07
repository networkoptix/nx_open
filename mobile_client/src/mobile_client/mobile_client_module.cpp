#include "mobile_client_module.h"

#include <QtGui/QGuiApplication>

#include "core/resource_management/resource_pool.h"
#include "core/resource_management/resource_properties.h"
#include "core/resource_management/status_dictionary.h"
#include "core/resource/camera_user_attribute_pool.h"
#include "core/resource/media_server_user_attributes.h"
#include "core/resource_management/server_additional_addresses_dictionary.h"
#include "common/common_module.h"
#include "api/app_server_connection.h"
#include "api/session_manager.h"
#include "api/global_settings.h"
#include "api/runtime_info_manager.h"
#include "api/simple_network_proxy_factory.h"
#include "mobile_client/mobile_client_meta_types.h"
#include "mobile_client/mobile_client_settings.h"
#include "utils/common/long_runnable.h"
#include "utils/common/app_info.h"
#include "utils/common/synctime.h"
#include "plugins/resource/server_camera/server_camera_factory.h"
#include "mobile_client/mobile_client_message_processor.h"
#include "watchers/user_watcher.h"

#include "version.h"

QnMobileClientModule::QnMobileClientModule(QObject *parent) :
    QObject(parent)
{
    Q_INIT_RESOURCE(mobile_client);
    Q_INIT_RESOURCE(appserver2);

    QnMobileClientMetaTypes::initialize();

    /* Set up application parameters so that QSettings know where to look for settings. */
    QGuiApplication::setOrganizationName(lit(QN_ORGANIZATION_NAME));
    QGuiApplication::setApplicationName(lit(QN_APPLICATION_NAME));
    QGuiApplication::setApplicationVersion(lit(QN_APPLICATION_VERSION));

    /* Init singletons. */
    QnCommonModule *common = new QnCommonModule(this);
    common->setModuleGUID(QnUuid::createUuid());

    common->store<QnMobileClientSettings>(new QnMobileClientSettings);
    common->store<QnSessionManager>(new QnSessionManager());

    common->store<QnCameraUserAttributePool>(new QnCameraUserAttributePool());
    common->store<QnMediaServerUserAttributesPool>(new QnMediaServerUserAttributesPool());
    common->store<QnSyncTime>(new QnSyncTime());

    common->store<QnResourcePropertyDictionary>(new QnResourcePropertyDictionary());
    common->store<QnResourceStatusDictionary>(new QnResourceStatusDictionary());
    common->store<QnServerAdditionalAddressesDictionary>(new QnServerAdditionalAddressesDictionary());

    common->store<QnLongRunnablePool>(new QnLongRunnablePool());
    common->store<QnGlobalSettings>(new QnGlobalSettings());
    common->store<QnMobileClientMessageProcessor>(new QnMobileClientMessageProcessor());
    common->store<QnRuntimeInfoManager>(new QnRuntimeInfoManager());
    common->store<QnServerCameraFactory>(new QnServerCameraFactory());

    common->store<QnUserWatcher>(new QnUserWatcher());

    QNetworkProxyFactory::setApplicationProxyFactory(new QnSimpleNetworkProxyFactory());

    QnAppServerConnectionFactory::setDefaultFactory(QnServerCameraFactory::instance());
}

QnMobileClientModule::~QnMobileClientModule() {
    QNetworkProxyFactory::setApplicationProxyFactory(nullptr);
}
