#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtQml/QQmlEngine>
#include <QtQml/QtQml>
#include <QtQml/QQmlFileSelector>
#include <QtQuick/QQuickWindow>

#include <time.h>

#include "api/app_server_connection.h"
#include "api/runtime_info_manager.h"
#include "nx_ec/ec2_lib.h"
#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/mobile_client_camera_factory.h"
#include "utils/common/app_info.h"
#include "utils/common/log.h"

#include "context/context.h"
#include "mobile_client/mobile_client_module.h"

#include "ui/color_theme.h"
#include "ui/resolution_util.h"
#include "ui/camera_thumbnail_provider.h"
#include "ui/icon_provider.h"
#include "ui/window_utils.h"
#include "ui/texture_size_helper.h"
#include "camera/camera_thumbnail_cache.h"

#include "version.h"

int runUi(QGuiApplication *application) {
    QScopedPointer<QnCameraThumbnailCache> thumbnailsCache(new QnCameraThumbnailCache());
    QnCameraThumbnailProvider *thumbnailProvider = new QnCameraThumbnailProvider();
    thumbnailProvider->setThumbnailCache(thumbnailsCache.data());

    QnCameraThumbnailProvider *activeCameraThumbnailProvider = new QnCameraThumbnailProvider();

#ifndef Q_OS_IOS
    QFont font;
    font.setFamily(lit("Roboto"));
    QGuiApplication::setFont(font);
#endif

    QnContext context;

    QnResolutionUtil::DensityClass densityClass = QnResolutionUtil::instance()->densityClass();
    qDebug() << "Starting with density class: " << QnResolutionUtil::densityName(densityClass);

    QFileSelector fileSelector;
    fileSelector.setExtraSelectors(QStringList() << QnResolutionUtil::densityName(densityClass));

    QnIconProvider *iconProvider = new QnIconProvider(&fileSelector);

    QStringList colorThemeFiles;
    colorThemeFiles.append(fileSelector.select(lit(":/color_theme.json")));
    if (QFile::exists(lit(":/color_theme_custom.json")))
        colorThemeFiles.append(fileSelector.select(lit(":/color_theme_custom.json")));
    context.colorTheme()->readFromFiles(colorThemeFiles);
    qApp->setPalette(context.colorTheme()->palette());

    QQmlEngine engine;
    QQmlFileSelector qmlFileSelector(&engine);
    qmlFileSelector.setSelector(&fileSelector);

#ifdef Q_OS_IOS
    engine.addImportPath(lit("qt_qml"));
#endif

    engine.addImageProvider(lit("thumbnail"), thumbnailProvider);
    engine.addImageProvider(lit("active"), activeCameraThumbnailProvider);
    engine.addImageProvider(lit("icon"), iconProvider);
    engine.rootContext()->setContextObject(&context);
    engine.rootContext()->setContextProperty(lit("screenPixelMultiplier"), QnResolutionUtil::instance()->densityMultiplier());

    QQmlComponent mainComponent(&engine, QUrl(lit("qrc:///qml/main.qml")));
    QScopedPointer<QQuickWindow> mainWindow(qobject_cast<QQuickWindow*>(mainComponent.create()));

    QScopedPointer<QnTextureSizeHelper> textureSizeHelper(new QnTextureSizeHelper(mainWindow.data()));

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    if (mainWindow) {
        mainWindow->setWidth(480);
        mainWindow->setHeight(800);
    }
#endif

    if (!mainComponent.errors().isEmpty())
        qWarning() << mainComponent.errorString();

    QObject::connect(&engine, &QQmlEngine::quit, application, &QGuiApplication::quit);

    prepareWindow();

    return application->exec();
}

int runApplication(QGuiApplication *application) {
    // these functions should be called in every thread that wants to use rand() and qrand()
    srand(time(NULL));
    qsrand(time(NULL));

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(getConnectionFactory(Qn::PT_MobileClient)); // TODO: #dklychkov check connection type
    ec2::ResourceContext resourceContext(
        QnMobileClientCameraFactory::instance(),
        qnResPool,
        qnResTypePool);
    ec2ConnectionFactory->setContext(resourceContext);
    QnAppServerConnectionFactory::setEC2ConnectionFactory(ec2ConnectionFactory.get());

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = Qn::PT_MobileClient; // TODO: #dklychkov check connection type
    runtimeData.peer.dataFormat = Qn::JsonFormat;
    runtimeData.brand = QnAppInfo::productNameShort();
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);

    int result = runUi(application);

//    QnResource::stopCommandProc();
    QnAppServerConnectionFactory::setEc2Connection(ec2::AbstractECConnectionPtr());
    QnAppServerConnectionFactory::setUrl(QUrl());

    return result;
}

void initLog() {
    QnLog::initLog(lit("INFO"));
}

int main(int argc, char *argv[]) {
    QGuiApplication application(argc, argv);

    initLog();

    QnMobileClientModule mobile_client;
    Q_UNUSED(mobile_client)

    int result = runApplication(&application);

    return result;
}
