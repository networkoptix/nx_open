// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <time.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <QtCore/QDir>
#include <QtCore/QScopedPointer>
#include <QtGui/QCursor>
#include <QtGui/QDesktopServices>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlFileSelector>
#include <QtQml/QtQml>
#include <QtQuick/QQuickWindow>
#include <QtQuickControls2/QQuickStyle>
#include <QtWebView/QtWebView>

#include <camera/camera_thumbnail_cache.h>
#include <common/static_common_module.h>
#include <context/context.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <core/storage/file_storage/qtfile_storage_resource.h>
#include <mobile_client/mobile_client_module.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_startup_parameters.h>
#include <mobile_client/mobile_client_ui_controller.h>
#include <mobile_client/mobile_client_uri_handler.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/cloud/db/api/oauth_data.h>
#include <nx/kit/output_redirector.h>
#include <nx/media/decoder_registrar.h>
#include <nx/media/media_fwd.h>
#include <nx/media/video_decoder_registry.h>
#include <nx/network/system_socket.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/ios_device_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/rlimit.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/client/core/media/watermark_image_provider.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/font_loader.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <ui/camera_thumbnail_provider.h>
#include <ui/texture_size_helper.h>
#include <ui/window_utils.h>
#include <utils/intent_listener_android.h>

#include "gl_context_synchronizer.h"
#include "ini.h"

#include <nx/vms/client/core/testkit/testkit.h>
#include <nx/vms/client/core/testkit/http_server.h>

using namespace nx::vms::client;
using namespace nx::mobile_client;
using namespace std::chrono;
using namespace nx::media;

QString getTargetFontFamiliy(const QString& defaultFontName, const QString& applicationLocaleCode)
{
    if (!nx::build_info::isAndroid())
        return defaultFontName;

    // In Android Qt chooses a fallback font for some languages according to the value of
    // `QLocale::system()`. As we can have situations when the system locale differs from the
    // mobile client locale, we need to exlicitly set a font which supports the correct character
    // set.

    struct FontData
    {
        QString fontLanguageCode;
        QFontDatabase::WritingSystem writingSystem;
        QString fallbackFontFamily;
    };

    static const std::unordered_map<QString, FontData> sLocaleCodeToFontData = {
        {"ja_JP", {"JP", QFontDatabase::Japanese, "Noto Sans CJK JP"}},
        {"zh_CN", {"TC", QFontDatabase::TraditionalChinese, "Noto Sans CJK TC"}},
        {"zh_TW", {"SC", QFontDatabase::SimplifiedChinese, "Noto Sans CJK SC"}},
        {"ko_KR", {"KR", QFontDatabase::Korean, "Noto Sans CJK KR"}}};

    const auto it = sLocaleCodeToFontData.find(applicationLocaleCode);
    if (it == sLocaleCodeToFontData.end())
        return defaultFontName;

    const auto systemLocale = QLocale::system();
    const QLocale appLocale(applicationLocaleCode);
    if (systemLocale.language() == appLocale.language()
        && systemLocale.country() == appLocale.country())
    {
        // Fallback fonts are applied correctly.
        return defaultFontName;
    }

    QString result = defaultFontName;
    const auto families = QFontDatabase::families(it->second.writingSystem);

    for (const auto& family: families)
    {
        if (family == it->second.fallbackFontFamily)
            return family; //< We found the fallback font.

        if (family.startsWith("Noto") && family.endsWith(it->second.fontLanguageCode))
            result = family; //< We're OK with any standard language-related "Noto" font.
    }

    return result;
}

int runUi(QGuiApplication* application)
{
    QScopedPointer<QnCameraThumbnailCache> thumbnailsCache(new QnCameraThumbnailCache());
    QnCameraThumbnailProvider *thumbnailProvider = new QnCameraThumbnailProvider();
    thumbnailProvider->setThumbnailCache(thumbnailsCache.data());

    QnCameraThumbnailProvider *activeCameraThumbnailProvider = new QnCameraThumbnailProvider();

    QString fontsDir = QDir(qApp->applicationDirPath()).absoluteFilePath("fonts");
    if (nx::build_info::isAndroid())
        fontsDir = "assets:/fonts";

    nx::vms::client::core::FontLoader::loadFonts(fontsDir);
    const auto targetFont = QFont(getTargetFontFamiliy("Roboto",
        nx::vms::client::core::appContext()->coreSettings()->locale()));
    QGuiApplication::setFont(targetFont);

    const auto context = qnMobileClientModule->context();

    QScopedPointer<QnMobileClientUriHandler> uriHandler(new QnMobileClientUriHandler(context));
    for (const auto& scheme: uriHandler->supportedSchemes())
        QDesktopServices::setUrlHandler(scheme, uriHandler.data(), uriHandler->handlerMethodName());

    QStringList selectors;

    QFileSelector fileSelector;
    fileSelector.setExtraSelectors(selectors);

    const auto engine = core::appContext()->qmlEngine();
    QQmlFileSelector qmlFileSelector(engine);
    qmlFileSelector.setSelector(&fileSelector);

    engine->addImageProvider("thumbnail", thumbnailProvider);
    engine->addImageProvider("active", activeCameraThumbnailProvider);
    engine->addImageProvider(nx::vms::client::core::WatermarkImageProvider::name(),
        new nx::vms::client::core::WatermarkImageProvider());

    engine->rootContext()->setContextObject(context);
    engine->setObjectOwnership(context, QQmlEngine::CppOwnership);

    QQmlComponent mainComponent(engine, QUrl("main.qml"));
    const QScopedPointer<QQuickWindow, QScopedPointerDeleteLater> mainWindow(
        qobject_cast<QQuickWindow*>(mainComponent.create()));
    engine->setParent(mainWindow.data());

    QScopedPointer<QnTextureSizeHelper> textureSizeHelper(
        new QnTextureSizeHelper(mainWindow.data()));

    if (!nx::build_info::isMobile() && mainWindow)
    {
        mainWindow->setWidth(380);
        mainWindow->setHeight(650);
    }

    if (!mainComponent.errors().isEmpty())
    {
        qWarning() << mainComponent.errorString();
        return 1;
    }

    qnMobileClientModule->setMainWindow(mainWindow.data());
    QObject::connect(engine, &QQmlEngine::quit, application, &QGuiApplication::quit);

    prepareWindow();

    QSize maxFfmpegResolution = qnSettings->maxFfmpegResolution();
    QSize maxFfmpegHevcResolution = maxFfmpegResolution;

    if (maxFfmpegResolution.isEmpty())
    {
        // Use platform-dependent defaults.
        if (nx::build_info::isArm() && !nx::build_info::isMacOsX())
        {
            if (nx::build_info::isIos())
            {
                // Since in iOS we may turn hardware decoding on/off we set maximum reasonable
                // resolution here.
                static const QSize kDci4kResolution(4096, 2160);
                maxFfmpegResolution = kDci4kResolution;
                maxFfmpegHevcResolution = kDci4kResolution;
            }
            else
            {
                maxFfmpegResolution = QSize(1920, 1088);
                maxFfmpegHevcResolution = QSize(800, 600);
            }
        }
    }

    QMap<int, QSize> maxFfmpegResolutions;
    maxFfmpegResolutions[(int) AV_CODEC_ID_NONE] = maxFfmpegResolution;
    maxFfmpegResolutions[(int) AV_CODEC_ID_H265] = maxFfmpegHevcResolution;

    nx::media::DecoderRegistrar::registerDecoders(maxFfmpegResolutions);

    #if defined(Q_OS_ANDROID)
        QUrl initialIntentData = getInitialIntentData();
        if (initialIntentData.isValid())
            QDesktopServices::openUrl(initialIntentData);
    #endif

    return application->exec();
}

int runApplication(QGuiApplication* application)
{
    int result = runUi(application);
    return result;
}

class TcpLogWriterOut : public nx::log::AbstractWriter
{
public:
    TcpLogWriterOut(const nx::network::SocketAddress& targetAddress) :
        m_targetAddress(targetAddress)
    {
    }

    virtual void write(nx::log::Level /*level*/, const QString& message) override
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

bool initLogFromConfigFile()
{
    static const QString kLogConfig("mobile_client_log");

    const QDir iniFilesDir(nx::kit::IniConfig::iniFilesDir());
    const QString logConfigFile(iniFilesDir.absoluteFilePath(kLogConfig + ".ini"));

    if (!QFileInfo(logConfigFile).exists())
        return false;

    NX_INFO(NX_SCOPE_TAG, "Log is initialized from the %1", logConfigFile);
    NX_INFO(NX_SCOPE_TAG, "Log options from settings are ignored!");

    return nx::log::initializeFromConfigFile(
        logConfigFile,
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/log",
        /*applicationName*/ "mobile_client",
        qApp->applicationFilePath());
}

void initLogFromStartupParams(const QString& logLevel)
{
    nx::log::Settings logSettings;
    logSettings.loggers.resize(1);
    logSettings.loggers.front().level.parse(logLevel);
    if (*ini().logLevel)
        logSettings.loggers.front().level.parse(QString::fromUtf8(ini().logLevel));

    logSettings.loggers.front().maxFileSizeB = 10 * 1024 * 1024;
    logSettings.loggers.front().maxVolumeSizeB = 50 * 1024 * 1024;
    logSettings.loggers.front().logBaseName = *ini().logFile
        ? QString::fromUtf8(ini().logFile)
        : nx::build_info::isAndroid()
            ? "-"
            : (QString::fromUtf8(nx::kit::IniConfig::iniFilesDir()) + "mobile_client");

    std::unique_ptr<nx::log::AbstractWriter> logWriter;

    const QString tcpLogAddress(QLatin1String(ini().tcpLogAddress));
    if (!tcpLogAddress.isEmpty())
    {
        auto params = tcpLogAddress.split(':');
        auto address = params[0];
        int port = 7001;
        if (params.size() >= 2)
            port = params[1].toInt();
        logWriter = std::make_unique<TcpLogWriterOut>(nx::network::SocketAddress(address, port));
    }

    nx::log::setMainLogger(
        nx::log::buildLogger(
            logSettings,
            /*applicationName*/ "mobile_client",
            QString(),
            std::set<nx::log::Filter>(),
            std::move(logWriter)));
}

bool initializeLogging(const QnMobileClientStartupParameters& startupParameters)
{
    if (initLogFromConfigFile())
        return true;

    if (!ini().enableLog)
        return false;

    initLogFromStartupParams(startupParameters.logLevel);
    return true;
}

void processStartupParams(const QnMobileClientStartupParameters& startupParameters)
{
    if (startupParameters.url.isValid())
        NX_DEBUG(NX_SCOPE_TAG, "--url: %1", startupParameters.url);

    const auto authData = core::appContext()->coreSettings()->cloudAuthData();
    if (!authData.empty())
    {
        core::appContext()->cloudStatusWatcher()->setAuthData(
            authData,
            core::CloudStatusWatcher::AuthMode::initial);
    }
}

int MOBILE_CLIENT_EXPORT main(int argc, char *argv[])
{
    QQuickStyle::setStyle("Basic");

    nx::utils::rlimit::setMaxFileDescriptors(1024);

    if (nx::build_info::isMacOsX())
    {
        // We do not rely on Mac OS OpenGL implementation-related throttling.
        // Otherwise all animations go faster.
        qputenv("QSG_RENDER_LOOP", "basic");
    }

    // Disables platform-specific text input handles since we have our custom ones.
    if (nx::build_info::isAndroid())
        qputenv("QT_QPA_NO_TEXT_HANDLES", "1");

    nx::kit::OutputRedirector::ensureOutputRedirection();

    // TODO: #muskov Introduce a convenient cross-platform entity for crash handlers.
    #if defined(Q_OS_WIN)
        AllowSetForegroundWindow(ASFW_ANY);
        win32_exception::installGlobalUnhandledExceptionHandler();
    #endif

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QtWebView::initialize();
    QGuiApplication application(argc, argv);

    // Set up application parameters so that QSettings know where to look for settings.
    QGuiApplication::setOrganizationName(nx::branding::company());
    QGuiApplication::setApplicationName(nx::branding::mobileClientInternalName());
    QGuiApplication::setApplicationDisplayName(nx::branding::mobileClientDisplayName());
    QGuiApplication::setApplicationVersion(nx::build_info::mobileClientVersion());

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QnMobileClientStartupParameters startupParams(application);

    ini().reload();
    bool loggingIsInitialized = initializeLogging(startupParams);

    QnMobileClientSettings settings;
    const auto applicationContext = std::make_unique<mobile::ApplicationContext>();
    const auto mobileClient = std::make_unique<QnMobileClientModule>(
        applicationContext->currentSystemContext(),
        startupParams);

    if (!loggingIsInitialized)
    {
        const auto level = nx::log::levelFromString(qnSettings->logLevel());
        if (level != nx::log::mainLogger()->defaultLevel())
            nx::log::mainLogger()->setDefaultLevel(level);
    }

    mobileClient->initDesktopCamera();
    mobileClient->startLocalSearches();

    qnSettings->setStartupParameters(startupParams);
    processStartupParams(startupParams);

    applicationContext->storagePluginFactory()->registerStoragePlugin(
        QLatin1String("file"), QnQtFileStorageResource::instance, true);

    #if defined(Q_OS_ANDROID)
        registerIntentListener();
    #endif

    std::unique_ptr<nx::vms::client::core::testkit::HttpServer> testkitServer;
    if (const int port = ini().clientWebServerPort; port > 0)
    {
        nx::vms::client::core::testkit::TestKit::setup();
        testkitServer = std::make_unique<nx::vms::client::core::testkit::HttpServer>(port);
    }

    int result = runApplication(&application);

    return result;
}
