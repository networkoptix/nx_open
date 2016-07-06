#include <QtCore/QDir>
#include <QtCore/QCommandLineParser>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QFont>
#include <QtGui/QOpenGLContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QtQml>
#include <QtQml/QQmlFileSelector>
#include <QtQuick/QQuickWindow>

#include <time.h>

#include <nx/utils/log/log.h>
#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>
#include <nx_ec/ec2_lib.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <utils/settings_migration.h>

#include <context/context.h>
#include <mobile_client/mobile_client_module.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_uri_handler.h>

#include <ui/camera_thumbnail_provider.h>
#include <ui/window_utils.h>
#include <ui/texture_size_helper.h>
#include <camera/camera_thumbnail_cache.h>
#include <ui/helpers/font_loader.h>
#include <ui/videowall_handler.h>
#include <utils/intent_listener_android.h>

#include <nx/media/decoder_registrar.h>
#include <resource_allocator.h>
#include "config.h"
using mobile_client::conf;

// TODO mike: REMOVE
#ifdef Q_OS_WIN
    #include <common/systemexcept_win32.h>
#endif
#ifdef Q_OS_LINUX
    #include <common/systemexcept_linux.h>
#endif

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

    QScopedPointer<QnMobileClientUriHandler> uriHandler(new QnMobileClientUriHandler());
    uriHandler->setUiController(context.uiController());
    for (const auto& scheme: uriHandler->supportedSchemes())
        QDesktopServices::setUrlHandler(scheme, uriHandler.data(), uriHandler->handlerMethodName());

    qnCommon->instance<QnVideowallHandler>()->setUiController(context.uiController());

    QStringList selectors;

    if (context.liteMode())
    {
        selectors.append(lit("lite"));
        qWarning() << "Starting in lite mode";
    }

    QFileSelector fileSelector;
    fileSelector.setExtraSelectors(selectors);

    QQmlEngine engine;
    auto basePath = qnSettings->basePath();
    if (!basePath.startsWith(lit("qrc:")))
        basePath = lit("file://") + QDir(basePath).absolutePath() + lit("/");
    context.setLocalPrefix(basePath);
    engine.setBaseUrl(QUrl(basePath + lit("qml/")));
    engine.addImportPath(basePath + lit("qml"));
    QQmlFileSelector qmlFileSelector(&engine);
    qmlFileSelector.setSelector(&fileSelector);

#ifdef Q_OS_IOS
    engine.addImportPath(lit("qt_qml"));
#endif

    engine.addImageProvider(lit("thumbnail"), thumbnailProvider);
    engine.addImageProvider(lit("active"), activeCameraThumbnailProvider);
    engine.rootContext()->setContextObject(&context);

    QQmlComponent mainComponent(&engine, QUrl(lit("main.qml")));
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
    {
        qWarning() << mainComponent.errorString();
        return 1;
    }

    QObject::connect(&engine, &QQmlEngine::quit, application, &QGuiApplication::quit);

    prepareWindow();
    std::shared_ptr<nx::media::AbstractResourceAllocator> allocator(new ResourceAllocator(
        mainWindow.data()));

    QSize maxFfmpegResolution = qnSettings->maxFfmpegResolution();
    if (maxFfmpegResolution.isEmpty())
    {
        // Use platform-dependent defaults.

#if defined(__arm__)

        if (QnAppInfo::armBox() == QString::fromLatin1("bpi"))
            maxFfmpegResolution = QSize(1280, 720);
        else
            maxFfmpegResolution = QSize(1920, 1080);

#elif defined(Q_OS_ANDROID) || defined(Q_OS_IOS)

        maxFfmpegResolution = QSize(1920, 1080);

#endif
    }

    nx::media::DecoderRegistrar::registerDecoders(
        allocator, maxFfmpegResolution, /*isTranscodingEnabled*/ !context.liteMode());

    return application->exec();
}

int runApplication(QGuiApplication *application, const QnUuid& videowallInstanceGuid) {
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
    if (!videowallInstanceGuid.isNull())
        runtimeData.videoWallInstanceGuid = videowallInstanceGuid;
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

void parseCommandLine(const QCoreApplication& application, QnUuid* outVideowallInstanceGuid)
{
    QCommandLineParser parser;

    const auto basePathOption = QCommandLineOption(
        lit("base-path"),
        lit("The directory which contains runtime ui resources: 'qml' and 'images'."),
        lit("basePath"));
    parser.addOption(basePathOption);

    const auto liteModeOption = QCommandLineOption(
        lit("lite-mode"),
        lit("Enable lite mode."));
    parser.addOption(liteModeOption);

    const auto urlOption = QCommandLineOption(
        lit("url"),
        lit("URL to be used for server connection instead of asking login/password."));
    parser.addOption(urlOption);

    const auto videowallInstanceGuidOption = QCommandLineOption(
        lit("videowall-instance-guid"),
        lit("GUID which is used to check Videowall Control messages."));
    parser.addOption(videowallInstanceGuidOption);

    parser.process(application);

    if (parser.isSet(basePathOption))
    {
        const auto basePath = parser.value(basePathOption);
        const auto path = QDir(basePath).absoluteFilePath(lit("qml/main.qml"));
        if (QFile::exists(path))
            qnSettings->setBasePath(basePath);
        else
            qWarning() << lit("File %1 doesn't exist. Loading from qrc...").arg(path);
    }

    if (parser.isSet(liteModeOption))
        qnSettings->setLiteMode(static_cast<int>(LiteModeType::LiteModeEnabled));

    if (parser.isSet(urlOption))
        qnSettings->setLastUsedUrl(parser.value(urlOption));

    if (parser.isSet(videowallInstanceGuidOption) && outVideowallInstanceGuid)
    {
        *outVideowallInstanceGuid = QnUuid::fromStringSafe(
            parser.value(videowallInstanceGuidOption));
    }
}

int main(int argc, char *argv[])
{
    #if defined(Q_OS_WIN)
        AllowSetForegroundWindow(ASFW_ANY);
        win32_exception::installGlobalUnhandledExceptionHandler();
    #endif
    #if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        linux_exception::installCrashSignalHandler();
    #endif

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QGuiApplication application(argc, argv);

    conf.reload();
    initLog();

    QnMobileClientModule mobile_client;
    Q_UNUSED(mobile_client);

    QnUuid videowallInstanceGuid;
    parseCommandLine(application, &videowallInstanceGuid);

    migrateSettings();

    #if defined(Q_OS_ANDROID)
        registerIntentListener();
    #endif

    int result = runApplication(&application, videowallInstanceGuid);

    return result;
}
