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
#include <nx/utils/log/log_initializer.h>
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
#include <nx/mobile_client/helpers/inter_client_message.h>
#include <nx/network/system_socket.h>

extern "C"
{
#include <libavcodec/avcodec.h>
}

#include <ini.h>

// TODO: #muskov Introduce a convenient cross-platform entity for crash handlers.
#include <nx/utils/crash_dump/systemexcept.h>

using namespace nx::mobile_client;
using namespace std::chrono;

/////


#include <utils/common/delayed.h>
#include <nx/network/app_info.h>
#include <nx/vms/utils/system_uri.h>

using nx::vms::utils::SystemUri;

static constexpr int kDelay = 5 * 1000;
static const auto cloudUser = lit("ynikitenkov@networkoptix.com");
static const auto password = lit("qweasd123");

QString authString(const QString& user, const QString& password)
{
    return lit("auth=%1").arg(SystemUri::Auth({user, password}).encode());
}

nx::utils::Url loginToCloudUrl(
    const QString& user = QString(),
    const QString& password = QString())
{
    static const auto kBase =
        []()
        {
            SystemUri result;
            result.setDomain(nx::network::AppInfo::defaultCloudHostName());
            result.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
            return result.toString();
        }();
    const auto result = lit("%1?%2").arg(kBase, authString(user, password));
    return nx::utils::Url(result);
}

void testUrl(QnMobileClientUriHandler* handler, const nx::utils::Url& url)
{
    const auto callback = [url, handler]()
    {
        qWarning() << url.toString();
        handler->handleUrl(url);
    };
    executeDelayed(callback, kDelay);
}

void testConnectToCloudCommand(QnMobileClientUriHandler* handler)
{
//    const auto cloudConnectionUrlWithUserOnly = loginToCloudUrl(cloudUser);
//    testUrl(handler, cloudConnectionUrlWithUserOnly);

    //    const auto cloudConnectionUrlWithPasswordOnly = loginToCloudUrl(QString(), password);
    //    testUrl(handler, cloudConnectionUrlWithPasswordOnly);

    //    const auto cloudConnectionUrlWithAuth = loginToCloudUrl(cloudUser, password);
    //    testUrl(handler, cloudConnectionUrlWithAuth);

    const auto cloudConnectionUrlWithWrongAuth = loginToCloudUrl(cloudUser, lit("123"));
    testUrl(handler, cloudConnectionUrlWithWrongAuth);
}

//================

nx::utils::Url clientCommandUrl(
    const QString& systemId = QString(),
    const QString& user = QString(),
    const QString& password = QString())
{
    const auto kBase =
        [systemId, user, password]()
        {
            SystemUri result;
            result.setDomain(nx::network::AppInfo::defaultCloudHostName());
            result.setClientCommand(SystemUri::ClientCommand::Client);
            result.setSystemId(systemId);
            result.setSystemAction(SystemUri::SystemAction::View);
            result.setResourceIds({
                QnUuid::fromStringSafe("04762367-751f-2727-25ca-9ae0dc6727de"),
                QnUuid::fromStringSafe("052c6604-e7a1-f8df-7cbe-891c9235d5e2"),
                QnUuid::fromStringSafe("0d118584-3d2d-ed7e-71da-bc08499daeaa")});
            result.setAuthenticator(user, password);
            return result.toString();
        }();

    const auto result = lit("%1&%2").arg(kBase);
    return nx::utils::Url(kBase);
}

void testClientCommand(QnMobileClientUriHandler* handler)
{
    static const auto systemId = lit("76386686-379d-4c4e-909b-19b03bb87dab");
    static const auto systemHost = lit("10.0.3.198:7001");
    const auto clientCommandWithoutAuth = clientCommandUrl(systemId, cloudUser, lit("333"));
    testUrl(handler, clientCommandWithoutAuth);
}

void testUrlHandler(QnMobileClientUriHandler* handler)
{
//    testConnectToCloudCommand(handler);
    testClientCommand(handler);
}

////

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

    QScopedPointer<QnMobileClientUriHandler> uriHandler(new QnMobileClientUriHandler(&context));
    testUrlHandler(uriHandler.data());
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

    const auto engine = qnClientCoreModule->mainQmlEngine();
    QQmlFileSelector qmlFileSelector(engine);
    qmlFileSelector.setSelector(&fileSelector);

    engine->addImageProvider(lit("thumbnail"), thumbnailProvider);
    engine->addImageProvider(lit("active"), activeCameraThumbnailProvider);
    engine->rootContext()->setContextObject(&context);

    QQmlComponent mainComponent(engine, QUrl(lit("main.qml")));
    QPointer<QQuickWindow> mainWindow(qobject_cast<QQuickWindow*>(mainComponent.create()));

    QScopedPointer<QnTextureSizeHelper> textureSizeHelper(
            new QnTextureSizeHelper(mainWindow.data()));

    if (!QnAppInfo::isMobile())
    {
        if (mainWindow)
        {
            if (context.liteMode() && !ini().disableFullScreen)
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

    QObject::connect(engine, &QQmlEngine::quit, application, &QGuiApplication::quit);

    prepareWindow();
    std::shared_ptr<nx::media::AbstractResourceAllocator> allocator(new ResourceAllocator(
        mainWindow));

    QSize maxFfmpegResolution = qnSettings->maxFfmpegResolution();
    QSize maxFfmpegHevcResolution = maxFfmpegResolution;
    if (maxFfmpegResolution.isEmpty())
    {
        // Use platform-dependent defaults.

        if (QnAppInfo::isArm())
        {
            if (QnAppInfo::isBpi() && !QnAppInfo::isMobile())
            {
                maxFfmpegResolution = QSize(1280, 720);
                maxFfmpegHevcResolution = QSize(640, 480);
            }
            else
            {
                maxFfmpegResolution = QSize(1920, 1080);
                maxFfmpegHevcResolution = QSize(800, 600);
            }
        }
    }

    QMap<int, QSize> maxFfmpegResolutions;
    maxFfmpegResolutions[(int) AV_CODEC_ID_NONE] = maxFfmpegResolution;
    maxFfmpegResolutions[(int) AV_CODEC_ID_H265] = maxFfmpegHevcResolution;

    nx::media::DecoderRegistrar::registerDecoders(
        allocator, maxFfmpegResolutions, /*isTranscodingEnabled*/ !context.liteMode());

    #if defined(Q_OS_ANDROID)
        QUrl initialIntentData = getInitialIntentData();
        if (initialIntentData.isValid())
            QDesktopServices::openUrl(initialIntentData);
    #endif

    QObject::connect(application, &QtSingleGuiApplication::messageReceived, mainWindow,
        [&context, mainWindow](const QString& serializedMessage)
        {
            NX_LOG(lit("Processing application message BEGIN: %1")
                .arg(serializedMessage), cl_logDEBUG1);

            using nx::client::mobile::InterClientMessage;

            auto message = InterClientMessage::fromString(serializedMessage);

            switch (message.command)
            {
                case InterClientMessage::Command::startCamerasMode:
                    context.uiController()->openResourcesScreen();
                    context.uiController()->connectToSystem(qnSettings->startupParameters().url);
                    mainWindow->update();
                    break;
                case InterClientMessage::Command::refresh:
                    mainWindow->update();
                    break;
                case InterClientMessage::Command::updateUrl:
                {
                    nx::utils::Url url(message.parameters);
                    if (url.isValid())
                        qnSettings->startupParameters().url = nx::utils::Url(message.parameters);
                    break;
                }
            }

            NX_LOG(lit("Processing application message END: %1")
                .arg(serializedMessage), cl_logDEBUG1);
        });

    return application->exec();
}

int runApplication(QtSingleGuiApplication* application)
{
    int result = runUi(application);
    QnAppServerConnectionFactory::setEc2Connection(ec2::AbstractECConnectionPtr());
    return result;
}


class TcpLogWriterOut : public nx::utils::log::AbstractWriter
{
public:
    TcpLogWriterOut(const nx::network::SocketAddress& targetAddress) :
        m_targetAddress(targetAddress)
    {
    }

    virtual void write(nx::utils::log::Level level, const QString& message) override
    {
        constexpr milliseconds kTimeout = seconds(3);
        if (!m_tcpSock)
        {
            m_tcpSock = std::make_unique<nx::network::TCPSocket>();
            auto ipV4Address = m_targetAddress.address.ipV4();
            if (!ipV4Address)
                return; //< Can't connect to non IPv4 address.
            if (!m_tcpSock->connect(
                nx::network::SocketAddress(*ipV4Address, m_targetAddress.port), kTimeout))
            {
                m_tcpSock.reset();
                return;
            }
            m_tcpSock->setSendTimeout(kTimeout);
        }
        QByteArray data = message.toUtf8();
        data.append('\n');
        if (m_tcpSock->send(data.data(), data.size()) < 1)
            m_tcpSock.reset(); //< Reconnect.
    }
private:
    std::unique_ptr<nx::network::AbstractStreamSocket> m_tcpSock;
    nx::network::SocketAddress m_targetAddress;
};

void initLog(const QString& logLevel)
{
    nx::utils::log::Settings logSettings;
    logSettings.level.parse(logLevel);
    if (*ini().logLevel)
        logSettings.level.parse(QString::fromUtf8(ini().logLevel));

    logSettings.maxFileSize = 10 * 1024 * 1024;
    logSettings.maxBackupCount = 5;

    if (ini().enableLog)
    {
        nx::utils::log::initialize(
            logSettings,
            /*applicationName*/ lit("mobile_client"),
            /*binaryPath*/ QString(),
            /*baseName*/
                *ini().logFile
                ? QString::fromUtf8(ini().logFile)
                : QnAppInfo::isAndroid()
                    ? lit("-")
                    : (QString::fromUtf8(nx::kit::IniConfig::iniFilesDir())
                        + lit("mobile_client")));
        const QString tcpLogAddress(QLatin1String(ini().tcpLogAddress));
        if (!tcpLogAddress.isEmpty())
        {
            auto params = tcpLogAddress.split(L':');
            auto address = params[0];
            int port = 7001;
            if (params.size() >= 2)
                port = params[1].toInt();
            nx::utils::log::mainLogger()->setWriter(
                std::make_unique<TcpLogWriterOut>(nx::network::SocketAddress(address, port)));
        }
    }

    const auto ec2logger = nx::utils::log::addLogger({QnLog::EC2_TRAN_LOG});
    if (ini().enableEc2TranLog)
    {
        nx::utils::log::initialize(
            logSettings,
            /*applicationName*/ lit("mobile_client"),
            /*binaryPath*/ QString(),
            /*baseName*/
                QnAppInfo::isAndroid()
                ? lit("-")
                : (QString::fromUtf8(nx::kit::IniConfig::iniFilesDir()) + lit("ec2_tran")),
            ec2logger);
    }
}

void processStartupParams(const QnMobileClientStartupParameters& startupParameters)
{
    if (ini().forceNonLiteMode)
        qnSettings->setLiteMode(static_cast<int>(LiteModeType::LiteModeDisabled));
    else if (startupParameters.liteMode || ini().forceLiteMode)
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

    using nx::client::mobile::InterClientMessage;

    auto sendApplicationMessage =
        [&application](const InterClientMessage& message)
        {
            const auto serializedMessage = message.toString();

            NX_LOG(lit("BEGIN Sending application message: %1")
                .arg(serializedMessage), cl_logDEBUG1);

            application.sendMessage(serializedMessage);

            NX_LOG(lit("END Sending application message: %1")
                .arg(serializedMessage), cl_logDEBUG1);
        };

    if (application.isRunning())
    {
        sendApplicationMessage(InterClientMessage(
            InterClientMessage::Command::updateUrl, startupParams.url.toString()));

        if (startupParams.autoLoginMode == AutoLoginMode::Enabled)
            sendApplicationMessage(InterClientMessage::Command::startCamerasMode);
        else
            sendApplicationMessage(InterClientMessage::Command::refresh);

        return 0;
    }

    ini().reload();
    initLog(startupParams.logLevel);

    QnStaticCommonModule staticModule(Qn::PT_MobileClient, QnAppInfo::brand(),
        QnAppInfo::customizationName());
    Q_UNUSED(staticModule);

    QnMobileClientModule mobile_client(startupParams);
    mobile_client.initDesktopCamera();
    mobile_client.startLocalSearches();
    Q_UNUSED(mobile_client);

    qnSettings->setStartupParameters(startupParams);
    processStartupParams(startupParams);

    #if defined(Q_OS_ANDROID)
        registerIntentListener();
    #endif

    int result = runApplication(&application);

    return result;
}
