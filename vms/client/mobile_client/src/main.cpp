#include <QtCore/QScopedPointer>
#include <QtCore/QDir>
#include <QtGui/QFont>
#include <QtGui/QDesktopServices>
#include <QtQml/QQmlEngine>
#include <QtQml/QtQml>
#include <QtQml/QQmlFileSelector>
#include <QtQuick/QQuickWindow>
#include <QtGui/QCursor>
#include <QtGui/QScreen>

#include <qtsingleguiapplication.h>

#include <time.h>

#include <nx/kit/output_redirector.h>

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
#include <nx/client/core/utils/font_loader.h>
#include <utils/intent_listener_android.h>
#include <handlers/lite_client_handler.h>

#include <nx/media/ios_device_info.h>
#include <gl_context_synchronizer.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/std/cpp14.h>

#include <nx/media/media_fwd.h>
#include <nx/media/decoder_registrar.h>
#include <nx/media/video_decoder_registry.h>
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
using namespace nx::media;

int runUi(QtSingleGuiApplication* application)
{
    QScopedPointer<QnCameraThumbnailCache> thumbnailsCache(new QnCameraThumbnailCache());
    QnCameraThumbnailProvider *thumbnailProvider = new QnCameraThumbnailProvider();
    thumbnailProvider->setThumbnailCache(thumbnailsCache.data());

    QnCameraThumbnailProvider *activeCameraThumbnailProvider = new QnCameraThumbnailProvider();

    // TODO: #dklychkov Detect fonts dir for iOS.
    QString fontsDir = QDir(qApp->applicationDirPath()).absoluteFilePath("fonts");
    nx::vms::client::core::FontLoader::loadFonts(fontsDir);

    QFont font;
    font.setFamily("Roboto");
    QGuiApplication::setFont(font);

    const auto context = qnMobileClientModule->context();

    QScopedPointer<QnMobileClientUriHandler> uriHandler(new QnMobileClientUriHandler(context));
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
            liteClientHandler->setUiController(context->uiController());

            auto webAdminController = commonModule->store(new controllers::WebAdminController());
            webAdminController->setUiController(context->uiController());

            webChannel->registerObject("liteClientController", webAdminController);
        }
    }

    QStringList selectors;

    if (context->liteMode())
    {
        selectors.append("lite");
        qWarning() << "Starting in lite mode";
    }

    QFileSelector fileSelector;
    fileSelector.setExtraSelectors(selectors);

    const auto engine = qnClientCoreModule->mainQmlEngine();
    QQmlFileSelector qmlFileSelector(engine);
    qmlFileSelector.setSelector(&fileSelector);

    engine->addImageProvider("thumbnail", thumbnailProvider);
    engine->addImageProvider("active", activeCameraThumbnailProvider);
    engine->rootContext()->setContextObject(context);

    QQmlComponent mainComponent(engine, QUrl("main.qml"));
    QScopedPointer<QQuickWindow> mainWindow(qobject_cast<QQuickWindow*>(mainComponent.create()));

    QScopedPointer<QnTextureSizeHelper> textureSizeHelper(
            new QnTextureSizeHelper(mainWindow.data()));

    if (!QnAppInfo::isMobile())
    {
        if (mainWindow)
        {
            if (context->liteMode() && !ini().disableFullScreen)
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

    QSize maxFfmpegResolution = qnSettings->maxFfmpegResolution();
    QSize maxFfmpegHevcResolution = maxFfmpegResolution;

    const bool forceSoftwareOnlyDecoderForIPhone =
        #if defined(Q_OS_IOS)
            ini().forceSoftwareDecoderForIPhoneXs
            && iosDeviceInformation().majorVersion == IosDeviceInformation::iPhoneXs
            && iosDeviceInformation().type == IosDeviceInformation::Type::iPhone;
        #else
            false;
        #endif

    if (maxFfmpegResolution.isEmpty())
    {
        // Use platform-dependent defaults.

        if (QnAppInfo::isArm())
        {
            if (QnAppInfo::isNx1())
            {
                maxFfmpegResolution = QSize(1280, 720);
                maxFfmpegHevcResolution = QSize(640, 480);
            }
            else if (forceSoftwareOnlyDecoderForIPhone)
            {
                static const QSize kDci4kResolution(4096, 2160);
                maxFfmpegResolution = kDci4kResolution;
                maxFfmpegHevcResolution = kDci4kResolution;
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
        maxFfmpegResolutions,
        /*isTranscodingEnabled*/ !context->liteMode(),
        !forceSoftwareOnlyDecoderForIPhone);

    #if defined(Q_OS_ANDROID)
        QUrl initialIntentData = getInitialIntentData();
        if (initialIntentData.isValid())
            QDesktopServices::openUrl(initialIntentData);
    #endif

    QObject::connect(application, &QtSingleGuiApplication::messageReceived, mainWindow.data(),
        [&context, &mainWindow](const QString& serializedMessage)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Processing application message BEGIN: %1", serializedMessage);

            using nx::client::mobile::InterClientMessage;

            auto message = InterClientMessage::fromString(serializedMessage);

            switch (message.command)
            {
                case InterClientMessage::Command::startCamerasMode:
                    context->uiController()->openResourcesScreen();
                    context->uiController()->connectToSystem(qnSettings->startupParameters().url);
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

            NX_DEBUG(NX_SCOPE_TAG, "Processing application message END: %1", serializedMessage);
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
    logSettings.loggers.resize(1);
    logSettings.loggers.front().level.parse(logLevel);
    if (*ini().logLevel)
        logSettings.loggers.front().level.parse(QString::fromUtf8(ini().logLevel));

    logSettings.loggers.front().maxFileSize = 10 * 1024 * 1024;
    logSettings.loggers.front().maxBackupCount = 5;
    logSettings.loggers.front().logBaseName = *ini().logFile
        ? QString::fromUtf8(ini().logFile)
        : QnAppInfo::isAndroid()
            ? "-"
            : (QString::fromUtf8(nx::kit::IniConfig::iniFilesDir()) + "mobile_client");

    if (ini().enableLog)
    {
        std::unique_ptr<nx::utils::log::AbstractWriter> logWriter;

        const QString tcpLogAddress(QLatin1String(ini().tcpLogAddress));
        if (!tcpLogAddress.isEmpty())
        {
            auto params = tcpLogAddress.split(L':');
            auto address = params[0];
            int port = 7001;
            if (params.size() >= 2)
                port = params[1].toInt();
            logWriter = std::make_unique<TcpLogWriterOut>(nx::network::SocketAddress(address, port));
        }

        nx::utils::log::setMainLogger(
            nx::utils::log::buildLogger(
                logSettings,
                /*applicationName*/ "mobile_client",
                QString(),
                std::set<nx::utils::log::Filter>(),
                std::move(logWriter)));
    }
}

void processStartupParams(const QnMobileClientStartupParameters& startupParameters)
{
    if (ini().forceNonLiteMode)
        qnSettings->setLiteMode(static_cast<int>(LiteModeType::LiteModeDisabled));
    else if (startupParameters.liteMode || ini().forceLiteMode)
        qnSettings->setLiteMode(static_cast<int>(LiteModeType::LiteModeEnabled));

    if (startupParameters.url.isValid())
        NX_DEBUG(NX_SCOPE_TAG, "--url: %1", startupParameters.url);

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
	nx::kit::OutputRedirector::ensureOutputRedirection();

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

            NX_DEBUG(NX_SCOPE_TAG, "BEGIN Sending application message: %1", serializedMessage);

            application.sendMessage(serializedMessage);

            NX_DEBUG(NX_SCOPE_TAG, "END Sending application message: %1", serializedMessage);
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

    QnStaticCommonModule staticModule(nx::vms::api::PeerType::mobileClient, QnAppInfo::brand(),
        QnAppInfo::customizationName());

    QnMobileClientModule mobile_client(startupParams);
    mobile_client.initDesktopCamera();
    mobile_client.startLocalSearches();

    qnSettings->setStartupParameters(startupParams);
    processStartupParams(startupParams);

    #if defined(Q_OS_ANDROID)
        registerIntentListener();
    #endif

    int result = runApplication(&application);

    return result;
}
