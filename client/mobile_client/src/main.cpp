#include <QtCore/QDir>
#include <QtGui/QGuiApplication>
#include <QtGui/QFont>
#include <QtQml/QQmlEngine>
#include <QtQml/QtQml>
#include <QtQml/QQmlFileSelector>
#include <QtQuick/QQuickWindow>

#include <time.h>

#include <nx/utils/log/log.h>
#include <api/app_server_connection.h>
#include <nx_ec/ec2_lib.h>
#include <common/common_module.h>
#include <utils/common/app_info.h>
#include <core/resource_management/resource_pool.h>

#include <context/context.h>
#include <mobile_client/mobile_client_module.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_uri_handler.h>
#include <mobile_client/mobile_client_startup_parameters.h>

#include <ui/camera_thumbnail_provider.h>
#include <ui/window_utils.h>
#include <ui/texture_size_helper.h>
#include <camera/camera_thumbnail_cache.h>
#include <ui/helpers/font_loader.h>
#include <utils/intent_listener_android.h>
#include <handlers/lite_client_handler.h>

#include <nx/media/decoder_registrar.h>
#include <resource_allocator.h>
#include <nx/utils/timer_manager.h>

#include <nx/mobile_client/webchannel/web_channel_server.h>
#include <nx/mobile_client/controllers/web_admin_controller.h>

#include "config.h"
using mobile_client::conf;

// TODO: #muskov Introduce a convenient cross-platform entity for crash handlers.
#include <common/systemexcept.h>

using namespace nx::mobile_client;

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

    if (qnSettings->isLiteClientModeEnabled())
    {
        auto webChannel = new webchannel::WebChannelServer();
        qnCommon->store<webchannel::WebChannelServer>(webChannel);

        auto liteClientHandler = new QnLiteClientHandler();
        liteClientHandler->setUiController(context.uiController());
        qnCommon->store<QnLiteClientHandler>(liteClientHandler);

        auto webAdminController = new controllers::WebAdminController();
        webAdminController->setUiController(context.uiController());
        qnCommon->store<controllers::WebAdminController>(webAdminController);

        webChannel->registerObject(lit("liteClientController"), webAdminController);
    }

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

    if (QnAppInfo::applicationPlatform() == lit("ios"))
        engine.addImportPath(lit("qt_qml"));

    engine.addImageProvider(lit("thumbnail"), thumbnailProvider);
    engine.addImageProvider(lit("active"), activeCameraThumbnailProvider);
    engine.rootContext()->setContextObject(&context);

    QQmlComponent mainComponent(&engine, QUrl(lit("main.qml")));
    QScopedPointer<QQuickWindow> mainWindow(qobject_cast<QQuickWindow*>(mainComponent.create()));

    QScopedPointer<QnTextureSizeHelper> textureSizeHelper(new QnTextureSizeHelper(mainWindow.data()));

    if (!QnAppInfo::isMobile())
    {
        if (mainWindow)
        {
            if (context.liteMode() && !conf.disableFullScreen)
            {
                mainWindow->showFullScreen();
            }
            else
            {
                mainWindow->setWidth(800);
                mainWindow->setHeight(600);
            }
        }
    }

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

        if (QnAppInfo::isArm())
        {
            if (QnAppInfo::isBpi())
                maxFfmpegResolution = QSize(1280, 720);
            else
                maxFfmpegResolution = QSize(1920, 1080);

            if (QnAppInfo::isMobile())
                maxFfmpegResolution = QSize(1920, 1080);
        }
    }

    nx::media::DecoderRegistrar::registerDecoders(
        allocator, maxFfmpegResolution, /*isTranscodingEnabled*/ !context.liteMode());

    #if defined(Q_OS_ANDROID)
        QUrl initialIntentData = getInitialIntentData();
        if (initialIntentData.isValid())
            QDesktopServices::openUrl(initialIntentData);
    #endif

    return application->exec();
}

int runApplication(QGuiApplication *application)
{
    NX_ASSERT(nx::utils::TimerManager::instance());
    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(
        getConnectionFactory(Qn::PT_MobileClient, nx::utils::TimerManager::instance()));

    QnAppServerConnectionFactory::setEC2ConnectionFactory(ec2ConnectionFactory.get());

    int result = runUi(application);

    QnAppServerConnectionFactory::setEc2Connection(ec2::AbstractECConnectionPtr());
    QnAppServerConnectionFactory::setUrl(QUrl());

    return result;
}

void initLog()
{
    QnLog::initLog(lit("INFO"));

    if (conf.enableEc2TranLog)
    {
        const auto logFileBaseName = QnAppInfo::isAndroid()
            ? lit("-")
            : QLatin1String(conf.tempPath()) + lit("ec2_tran");

        QnLog::instance(QnLog::EC2_TRAN_LOG)->create(
            logFileBaseName,
            /*DEFAULT_MAX_LOG_FILE_SIZE*/ 10*1024*1024,
            /*DEFAULT_MSG_LOG_ARCHIVE_SIZE*/ 5,
            cl_logDEBUG2);
    }

    if (conf.enableLog)
    {
        const auto logFileBaseName = QnAppInfo::isAndroid()
            ? lit("-")
            : QLatin1String(conf.tempPath()) + lit("mobile_client");

        QnLog::instance(QnLog::MAIN_LOG_ID)->create(
            logFileBaseName,
            /*DEFAULT_MAX_LOG_FILE_SIZE*/ 10*1024*1024,
            /*DEFAULT_MSG_LOG_ARCHIVE_SIZE*/ 5,
            cl_logDEBUG2);
    }
}

void processStartupParams(const QnMobileClientStartupParameters& startupParameters)
{
    if (!startupParameters.basePath.isEmpty())
    {
        const auto path = QDir(startupParameters.basePath).absoluteFilePath(lit("qml/main.qml"));
        if (QFile::exists(path))
            qnSettings->setBasePath(startupParameters.basePath);
        else
            qWarning() << lit("File %1 doesn't exist. Loading from qrc...").arg(path);
    }

    if (conf.forceNonLiteMode)
        qnSettings->setLiteMode(static_cast<int>(LiteModeType::LiteModeDisabled));
    else if (startupParameters.liteMode || conf.forceLiteMode)
        qnSettings->setLiteMode(static_cast<int>(LiteModeType::LiteModeEnabled));

    if (startupParameters.url.isValid())
        NX_LOG(lit("--url: %1").arg(startupParameters.url.toString()), cl_logDEBUG1);

    if (startupParameters.testMode)
    {
        qnSettings->setTestMode(true);
        qnSettings->setInitialTest(startupParameters.initialTest);
    }
}

int main(int argc, char *argv[])
{
    // TODO: #muskov Introduce a convenient cross-platform entity for crash handlers.
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

    QnMobileClientStartupParameters startupParams(application);

    QnMobileClientModule mobile_client(startupParams);
    Q_UNUSED(mobile_client);

    qnSettings->setStartupParameters(startupParams);
    processStartupParams(startupParams);

    #if defined(Q_OS_ANDROID)
        registerIntentListener();
    #endif

    int result = runApplication(&application);

    return result;
}
