#include <time.h>

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QFont>
#include <QtGui/QOpenGLContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QtQml>
#include <QtQuick/QQuickWindow>

#include <nx/utils/log/log.h>

#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>
#include <nx_ec/ec2_lib.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <utils/settings_migration.h>

#include "context/context.h"
#include "mobile_client/mobile_client_module.h"

#include "ui/color_theme.h"
#include "ui/resolution_util.h"
#include "ui/camera_thumbnail_provider.h"
#include "ui/icon_provider.h"
#include "ui/window_utils.h"
#include "ui/texture_size_helper.h"
#include "camera/camera_thumbnail_cache.h"
#include "ui/helpers/font_loader.h"

#include <nx/media/decoder_registrar.h>
#include "resource_allocator.h"
#include "config.h"
using mobile_client::conf;

int runUi(QGuiApplication *application) {
    QScopedPointer<QnCameraThumbnailCache> thumbnailsCache(new QnCameraThumbnailCache());
    QnCameraThumbnailProvider *thumbnailProvider = new QnCameraThumbnailProvider();
    thumbnailProvider->setThumbnailCache(thumbnailsCache.data());

    QnCameraThumbnailProvider *activeCameraThumbnailProvider = new QnCameraThumbnailProvider();

    // TODO: #dklychkov Detect fonts dir for iOS.
    QString fontsDir = QDir(qApp->applicationDirPath()).absoluteFilePath(lit("fonts"));
    QnFontLoader::loadFonts(fontsDir);

    QFont font;
    font.setFamily(lit("Roboto"));
    QGuiApplication::setFont(font);

    QnContext context;

    QnResolutionUtil::DensityClass densityClass = QnResolutionUtil::instance()->densityClass();
    qDebug() << "Starting with density class: " << QnResolutionUtil::densityName(densityClass);

    QStringList selectors;
    selectors.append(QnResolutionUtil::densityName(densityClass));

    if (context.liteMode())
    {
        selectors.append(lit("lite"));
        qWarning() << "Starting in lite mode";
    }

    QFileSelector fileSelector;
    fileSelector.setExtraSelectors(selectors);

    QnIconProvider *iconProvider = new QnIconProvider(&fileSelector);

    QStringList colorThemeFiles;
    colorThemeFiles.append(fileSelector.select(lit(":/color_theme.json")));
    if (QFile::exists(lit(":/color_theme_custom.json")))
        colorThemeFiles.append(fileSelector.select(lit(":/color_theme_custom.json")));
    context.colorTheme()->readFromFiles(colorThemeFiles);
    qApp->setPalette(context.colorTheme()->palette());

    QQmlEngine engine;
    engine.addImportPath(lit("qrc:///qml"));
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
    if (mainWindow)
    {
        if (context.liteMode())
        {
            mainWindow->showFullScreen();
        }
        else
        {
            mainWindow->setWidth(800);
            mainWindow->setHeight(600);
        }
    }
#endif

    if (!mainComponent.errors().isEmpty())
        qWarning() << mainComponent.errorString();

    QObject::connect(&engine, &QQmlEngine::quit, application, &QGuiApplication::quit);

    prepareWindow();
    std::shared_ptr<nx::media::AbstractResourceAllocator> allocator(new ResourceAllocator(
        mainWindow.data()));
    nx::media::DecoderRegistrar::registerDecoders(allocator);

    return application->exec();
}

int runApplication(QGuiApplication *application) {
    // these functions should be called in every thread that wants to use rand() and qrand()
    srand(time(NULL));
    qsrand(time(NULL));

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(getConnectionFactory(Qn::PT_MobileClient)); // TODO: #dklychkov check connection type

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

void initLog()
{
    QnLog::initLog(lit("INFO"));

    if (conf.enableEc2TranLog)
    {
        QnLog::instance(QnLog::EC2_TRAN_LOG)->create(
            QLatin1String(conf.tempPath()) + QLatin1String("ec2_tran"),
            /*DEFAULT_MAX_LOG_FILE_SIZE*/ 10*1024*1024,
            /*DEFAULT_MSG_LOG_ARCHIVE_SIZE*/ 5,
            cl_logDEBUG2);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QGuiApplication application(argc, argv);

    conf.reload();
    initLog();

    QnMobileClientModule mobile_client;
    Q_UNUSED(mobile_client)

    migrateSettings();

    int result = runApplication(&application);

    return result;
}
