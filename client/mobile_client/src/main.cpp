#include <QtCore/QDir>
#include <QtGui/QFont>
#include <QtQml/QQmlEngine>
#include <QtQml/QtQml>
#include <QtQml/QQmlFileSelector>
#include <QtQuick/QQuickWindow>
#include <QtGui/QCursor>
#include <QtGui/QScreen>

#include <qtsingleguiapplication.h>

#include <time.h>

#include <nx/utils/log/log.h>
#include <api/app_server_connection.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client_core/client_core_module.h>

#include <utils/common/app_info.h>
#include <core/resource_management/resource_pool.h>

#include <context/context.h>

#include <mobile_client/mobile_client_module.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_uri_handler.h>
#include <mobile_client/mobile_client_startup_parameters.h>
#include <mobile_client/mobile_client_ui_controller.h>

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
#include <nx/utils/std/cpp14.h>

#include <nx/mobile_client/webchannel/web_channel_server.h>
#include <nx/mobile_client/controllers/web_admin_controller.h>

#include "config.h"
using mobile_client::conf;

// TODO: #muskov Introduce a convenient cross-platform entity for crash handlers.
#include <nx/utils/crash_dump/systemexcept.h>

using namespace nx::mobile_client;

int runUi(QtSingleGuiApplication* application)
{
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
        auto commonModule = qnClientCoreModule->commonModule();

        auto preparingWebChannel = std::make_unique<webchannel::WebChannelServer>(
            qnSettings->webSocketPort());

        if (preparingWebChannel->isValid())
        {
            auto webChannel = commonModule->store(preparingWebChannel.release());
            qnSettings->setWebSocketPort(webChannel->serverPort());

            auto liteClientHandler = commonModule->store(new QnLiteClientHandler());
            liteClientHandler->setUiController(context.uiController());

            auto webAdminController = commonModule->store(new controllers::WebAdminController());
            webAdminController->setUiController(context.uiController());

            webChannel->registerObject(lit("liteClientController"), webAdminController);
        }
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
    QPointer<QQuickWindow> mainWindow(qobject_cast<QQuickWindow*>(mainComponent.create()));

    QScopedPointer<QnTextureSizeHelper> textureSizeHelper(new QnTextureSizeHelper(mainWindow.data()));

    if (!QnAppInfo::isMobile())
    {
        if (mainWindow)
        {
            if (context.liteMode() && !conf.disableFullScreen)
            {
                mainWindow->showFullScreen();
                if (const auto screen = mainWindow->screen())
                {
                    const auto size = screen->size();
                    QCursor::setPos(screen, size.width() / 2, size.height() / 2);
                }
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
        mainWindow));

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

    QObject::connect(application, &QtSingleGuiApplication::messageReceived, mainWindow,
        [&context, mainWindow](const QString& message)
        {
            NX_LOG(lit("Processing application message BEGIN: %1").arg(message), cl_logDEBUG1);
            if (message == lit("startCamerasMode"))
            {
                context.uiController()->openResourcesScreen();
                context.uiController()->connectToSystem(qnSettings->startupParameters().url);
                mainWindow->update();
            }
            else if (message == lit("refresh"))
            {
                mainWindow->update();
            }
            NX_LOG(lit("Processing application message END: %1").arg(message), cl_logDEBUG1);
        });

    return application->exec();
}

int runApplication(QtSingleGuiApplication* application)
{
    int result = runUi(application);
    QnAppServerConnectionFactory::setEc2Connection(ec2::AbstractECConnectionPtr());
    return result;
}

void initLog(const QString& logLevel)
{
    QnLog::initLog(logLevel);

    if (conf.enableEc2TranLog)
    {
        const auto logFileBaseName = QnAppInfo::isAndroid()
            ? lit("-")
            : QLatin1String(conf.tempPath()) + lit("ec2_tran");

        QnLog::instance(QnLog::EC2_TRAN_LOG)->create(
            logFileBaseName,
            /*DEFAULT_MAX_LOG_FILE_SIZE*/ 10 * 1024 * 1024,
            /*DEFAULT_MSG_LOG_ARCHIVE_SIZE*/ 5,
            cl_logDEBUG2);
    }

    if (conf.enableLog)
    {
        const auto logFileBaseName =
            conf.logFile && conf.logFile[0]
                ? QLatin1String(conf.logFile)
                : QnAppInfo::isAndroid()
                    ? lit("-")
                    : QLatin1String(conf.tempPath()) + lit("mobile_client");

        QnLog::instance(QnLog::MAIN_LOG_ID)->create(
            logFileBaseName,
            /*DEFAULT_MAX_LOG_FILE_SIZE*/ 10 * 1024 * 1024,
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

    if (startupParameters.autoLoginMode != AutoLoginMode::Undefined)
        qnSettings->setAutoLoginMode(static_cast<int>(startupParameters.autoLoginMode));

    if (startupParameters.testMode)
    {
        qnSettings->setTestMode(true);
        qnSettings->setInitialTest(startupParameters.initialTest);
    }

    qnSettings->setWebSocketPort(startupParameters.webSocketPort);
}

int main(int argc, char *argv[])
{
    // TODO: #muskov Introduce a convenient cross-platform entity for crash handlers.
    #if defined(Q_OS_WIN)
        AllowSetForegroundWindow(ASFW_ANY);
        win32_exception::installGlobalUnhandledExceptionHandler();
    #endif

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QtSingleGuiApplication application(argc, argv);

    QnMobileClientStartupParameters startupParams(application);

    if (application.isRunning())
    {
        if (startupParams.autoLoginMode == AutoLoginMode::Enabled)
        {
            NX_LOG(lit("BEGIN Sending application message: startCamerasMode"), cl_logDEBUG1);
            application.sendMessage(lit("startCamerasMode"));
            NX_LOG(lit("END Sending application message: startCamerasMode"), cl_logDEBUG1);
        }
        else
        {
            NX_LOG(lit("END Sending application message: refresh"), cl_logDEBUG1);
            application.sendMessage(lit("refresh"));
            NX_LOG(lit("BEGIN Sending application message: refresh"), cl_logDEBUG1);
        }
        return 0;
    }

    conf.reload();
    initLog(startupParams.logLevel);

    QnStaticCommonModule staticModule(Qn::PT_MobileClient, QnAppInfo::brand(),
        QnAppInfo::customizationName());
    Q_UNUSED(staticModule);

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
