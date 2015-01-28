#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtQml/QQmlEngine>
#include <QtQml/QtQml>
#include <QtQml/QQmlFileSelector>

#include <time.h>

#include "api/app_server_connection.h"
#include "api/session_manager.h"
#include "api/global_settings.h"
#include "api/runtime_info_manager.h"
#include "api/network_proxy_factory.h"
#include "nx_ec/ec2_lib.h"
#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource_management/resource_properties.h"
#include "core/resource_management/status_dictionary.h"
#include "core/resource/camera_user_attribute_pool.h"
#include "core/resource/media_server_user_attributes.h"
#include "utils/common/long_runnable.h"
#include "utils/common/app_info.h"
#include "utils/common/synctime.h"
#include "utils/common/log.h"
#include "plugins/resource/server_camera/server_camera_factory.h"

#include "context/context.h"
#include "mobile_client/mobile_client_module.h"
#include "mobile_client/mobile_client_message_processor.h"

#include "ui/color_theme.h"
#include "ui/resolution_util.h"

#include "version.h"

int runUi(QGuiApplication *application) {
    QnResolutionUtil::DensityClass densityClass = QnResolutionUtil::currentDensityClass();
    qDebug() << "Starting with density class: " << QnResolutionUtil::densityName(densityClass);

    QFileSelector fileSelector;
    fileSelector.setExtraSelectors(QStringList() << QnResolutionUtil::densityName(densityClass));

    QnContext context;

    context.colorTheme()->readFromFile(fileSelector.select(lit(":/color_theme.json")));
    qApp->setPalette(context.colorTheme()->palette());

    QQmlEngine engine;
    QQmlFileSelector qmlFileSelector(&engine);
    qmlFileSelector.setSelector(&fileSelector);

    engine.rootContext()->setContextObject(&context);
    engine.rootContext()->setContextProperty(lit("screenPixelMultiplier"), QnResolutionUtil::densityMultiplier(densityClass));

    QQmlComponent mainComponent(&engine, QUrl(lit("qrc:///qml/main.qml")));
    QScopedPointer<QObject> mainWindow(mainComponent.create());

    if (!mainComponent.errors().isEmpty())
        qWarning() << mainComponent.errorString();

    QObject::connect(&engine, &QQmlEngine::quit, application, &QGuiApplication::quit);

    return application->exec();
}

int runApplication(QGuiApplication *application) {
    // these functions should be called in every thread that wants to use rand() and qrand()
    srand(time(NULL));
    qsrand(time(NULL));

    QnSyncTime syncTime;
    QnResourcePropertyDictionary dictionary;
    QnResourceStatusDictionary statusDictionary;
    QScopedPointer<QnLongRunnablePool> runnablePool(new QnLongRunnablePool());
    QScopedPointer<QnGlobalSettings> globalSettings(new QnGlobalSettings());
    QScopedPointer<QnMobileClientMessageProcessor> mobileClientMessageProcessor(new QnMobileClientMessageProcessor());
    QScopedPointer<QnRuntimeInfoManager> runtimeInfoManager(new QnRuntimeInfoManager());
    QScopedPointer<QnServerCameraFactory> serverCameraFactory(new QnServerCameraFactory());

    //NOTE QNetworkProxyFactory::setApplicationProxyFactory takes ownership of object
    QNetworkProxyFactory::setApplicationProxyFactory(new QnNetworkProxyFactory());

    /* Initialize connections. */
    QnAppServerConnectionFactory::setClientGuid(qnCommon->moduleGUID().toString());
    QnAppServerConnectionFactory::setDefaultFactory(serverCameraFactory.data());

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(getConnectionFactory(Qn::PT_MobileClient)); // TODO: #dklychkov check connection type
    ec2::ResourceContext resourceContext(
        QnServerCameraFactory::instance(),
        qnResPool,
        qnResTypePool);
    ec2ConnectionFactory->setContext(resourceContext);
    QnAppServerConnectionFactory::setEC2ConnectionFactory(ec2ConnectionFactory.get());

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = Qn::PT_MobileClient; // TODO: #dklychkov check connection type
    runtimeData.brand = QnAppInfo::productNameShort();
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);

    int result = runUi(application);

    QnSessionManager::instance()->stop();
    QnResource::stopCommandProc();
    QnAppServerConnectionFactory::setEc2Connection(ec2::AbstractECConnectionPtr());
    QnAppServerConnectionFactory::setUrl(QUrl());

    QNetworkProxyFactory::setApplicationProxyFactory(0);

    return result;
}

void initLog() {
    QnLog::initLog(lit("INFO"));
}

int main(int argc, char *argv[]) {
    QGuiApplication application(argc, argv);

    initLog();

    QnMobileClientModule mobile_client(argc, argv);
    Q_UNUSED(mobile_client)

    QnSessionManager::instance();
    QScopedPointer<QnCameraUserAttributePool> cameraUserAttributePool(new QnCameraUserAttributePool());
    QScopedPointer<QnMediaServerUserAttributesPool> mediaServerUserAttributesPool(new QnMediaServerUserAttributesPool());
    QnResourcePool::initStaticInstance(new QnResourcePool());

    int result = runApplication(&application);

    delete QnResourcePool::instance();
    QnResourcePool::initStaticInstance(0);

    return result;
}
