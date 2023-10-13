// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application.h"

#ifdef Q_OS_LINUX
    #include <unistd.h>
#endif

#ifdef Q_WS_X11
    #include <X11/Xlib.h>
#endif

#if defined(Q_OS_MACOS)
    #include <sys/sysctl.h>
#endif

#include <iostream>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSettings>
#include <QtCore/QString>
#include <QtGui/QAction>
#include <QtGui/QDesktopServices>
#include <QtGui/QWindow>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtQuick/QQuickWindow>
#include <QtQuickControls2/QQuickStyle>
#include <QtWidgets/QApplication>

#include <client/client_module.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client/self_updater.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <nx/audio/audiodevice.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/kit/output_redirector.h>
#include <nx/media/decoder_registrar.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/speech_synthesizer/text_to_wave_server.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/log/log.h>
#include <nx/utils/rlimit.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/joystick/joystick_settings_action_handler.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/state/window_controller.h>
#include <nx/vms/client/desktop/state/window_geometry_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/eula_dialog.h>
#include <nx/vms/client/desktop/window_context.h>
#include <statistics/statistics_manager.h>
#include <ui/graphics/instruments/gl_checker_instrument.h>
#include <ui/statistics/modules/session_restore_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workaround/combobox_wheel_filter.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>

#if defined(Q_OS_MACOS)
    #include <ui/workaround/mac_utils.h>
#endif

#if defined(Q_OS_WIN)
    #include <QtGui/qpa/qplatformwindow_p.h>
#endif

namespace nx::vms::client::desktop {

namespace {

const int kSuccessCode = 0;
const QString kWindowGeometryData = "windowGeometry";
static constexpr int kStarDragDistance = 20;

void initApplication(const QnStartupParameters& startupParams)
{
    QThread::currentThread()->setPriority(QThread::HighestPriority);

    // Set up application parameters so that QSettings know where to look for settings.
    QApplication::setOrganizationName(nx::branding::company());
    QApplication::setApplicationName(nx::branding::desktopClientInternalName());
    QApplication::setApplicationDisplayName(nx::branding::desktopClientDisplayName());

    QString applicationVersion = !startupParams.engineVersion.isEmpty()
        ? startupParams.engineVersion : nx::build_info::vmsVersion();
    if (!nx::build_info::usedMetaVersion().isEmpty())
        applicationVersion += " " + nx::build_info::usedMetaVersion();

    QApplication::setApplicationVersion(applicationVersion);
    QApplication::setStartDragDistance(kStarDragDistance);

    // We don't want changes in desktop color settings to clash with our custom style.
    QApplication::setDesktopSettingsAware(false);
    QApplication::setQuitOnLastWindowClosed(true);
}

void sendCloudPortalConfirmation(const nx::vms::utils::SystemUri& uri, QObject* owner)
{
    QPointer<QNetworkAccessManager> manager(new QNetworkAccessManager(owner));
        QObject::connect(manager.data(), &QNetworkAccessManager::finished,
            [manager](QNetworkReply* reply)
            {
                reply->deleteLater();
                manager->deleteLater();
            });

        QUrl url(nx::toQString(nx::network::AppInfo::defaultCloudPortalUrl(
            nx::network::SocketGlobals::cloud().cloudHost())));
        url.setPath("/api/utils/visitedKey");

        const QJsonObject data{{QString("key"), uri.authKey()}};
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));

        manager->post(request, QJsonDocument(data).toJson(QJsonDocument::Compact));
}

Qt::WindowFlags calculateWindowFlags()
{
    if (qnRuntime->isAcsMode())
        return Qt::Window;

    Qt::WindowFlags result = nx::build_info::isMacOsX()
        ? Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint
        : Qt::Window | Qt::CustomizeWindowHint;

    if (qnRuntime->isVideoWallMode() && !ini().showBorderInVideoWallMode)
        result |= Qt::FramelessWindowHint | Qt::BypassWindowManagerHint;

    return result;
}

void initQmlGlyphCacheWorkaround()
{
    #if defined (Q_OS_MACOS)
        #if defined(__amd64)
            // Fixes issue with QML texts rendering observed on the MacBook laptops with
            // Nvidia GeForce GT 750M discrete graphic hardware and all M1 macs.
            size_t len = 0;
            sysctlbyname("hw.model", nullptr, &len, nullptr, 0);
            QByteArray hardwareModel(len, 0);
            sysctlbyname("hw.model", hardwareModel.data(), &len, nullptr, 0);

            // Detect running under Rosetta2.
            // https://developer.apple.com/documentation/apple-silicon/about-the-rosetta-translation-environment
            const auto processIsTranslated =
                []() -> bool
                {
                    int ret = 0;
                    size_t size = sizeof(ret);
                    if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) == -1)
                        return errno != ENOENT; //< Assume native only when ENOENT is returned.
                    return (bool) ret;
                };

            const bool glyphcacheWorkaround =
                hardwareModel.contains("MacBookPro11,2")
                || hardwareModel.contains("MacBookPro11,3")
                || processIsTranslated();
        #else
            // Currently all Apple Silicon macs require this.
            const bool glyphcacheWorkaround = true;
        #endif
    #else
        const bool glyphcacheWorkaround = false;
    #endif

    if (glyphcacheWorkaround)
        qputenv("QML_USE_GLYPHCACHE_WORKAROUND", "1");
}

} // namespace

int runApplicationInternal(QApplication* application, const QnStartupParameters& startupParams)
{
    ApplicationContext::Mode applicationMode = startupParams.selfUpdateMode
        ? ApplicationContext::Mode::selfUpdate
        : ApplicationContext::Mode::desktopClient;

    auto applicationContext = std::make_unique<ApplicationContext>(applicationMode, startupParams);

    // TODO: #sivanov Move QnClientModule contents to Application Context.
    QnClientModule client(startupParams);

    NX_INFO(NX_SCOPE_TAG, "IniConfig iniFilesDir: %1",  nx::kit::IniConfig::iniFilesDir());

    QnGLCheckerInstrument::checkGLHardware();
    client.initWebEngine();

    if (startupParams.customUri.isValid())
        sendCloudPortalConfirmation(startupParams.customUri, application);

    /* Running updater after QApplication and logging are initialized. */
    if (qnRuntime->isDesktopMode() && !startupParams.exportedMode)
    {
        /* All functionality is in the constructor. */
        SelfUpdater updater(startupParams);
    }

    /* Immediately exit if run in self-update mode. */
    if (startupParams.selfUpdateMode)
        return kSuccessCode;

    /* Initialize sound. */
    nx::audio::AudioDevice::instance()->setVolume(appContext()->localSettings()->audioVolume());

    qApp->installEventFilter(&HelpHandler::instance());

    // Hovered QComboBox changes its value when user scrolls a mouse wheel, even if the ComboBox
    // is not focused. It leads to weird and undesirable UI behaviour in some parts of the client.
    // We use a global Event Filter to prevent QComboBox instances from receiving Wheel events.
    ComboboxWheelFilter wheelFilter;
    qApp->installEventFilter(&wheelFilter);

    /* Create workbench context. */
    auto workbenchContext = std::make_unique<QnWorkbenchContext>(client.systemContext());

    // TODO: #sivanov Invert dependency. Workbench context should depend on Window context.
    auto windowContext = std::make_unique<WindowContext>(workbenchContext.get());
    applicationContext->addWindowContext(windowContext.get());

    #if defined(Q_OS_LINUX)
        qputenv("RESOURCE_NAME", nx::branding::brand().toUtf8());
    #endif

    // Dealing with EULA in videowall mode can make people frown.
    if (!qnRuntime->isVideoWallMode())
    {
        int accepted = appContext()->localSettings()->acceptedEulaVersion();
        int current = nx::branding::eulaVersion();
        const bool showEula = accepted < current && qgetenv("VMS_ACCEPT_EULA") != "YES";

        if (showEula && !EulaDialog::acceptEulaFromFile(":/license.html", current))
        {
            // We should exit completely.
            return 0;
        }
    }

    /* Create main window. */

    QScopedPointer<MainWindow> mainWindow(
        new MainWindow(workbenchContext.get(), /*parent*/ nullptr, calculateWindowFlags()));
    mainWindow->setAttribute(Qt::WA_QuitOnClose);

    auto hidJoystickManager = std::make_unique<joystick::JoystickSettingsActionHandler>(
        workbenchContext.get());

    #if defined(Q_OS_LINUX)
        qunsetenv("RESOURCE_NAME");
    #endif

    nx::media::DecoderRegistrar::registerDecoders({});

    // Window handle must exist before events processing. This is required to initialize the scene.
    volatile auto winId = mainWindow->winId();

    if (qnRuntime->isDesktopMode())
    {
        auto geometryManager = std::make_unique<WindowGeometryManager>(
            std::make_unique<WindowController>(mainWindow.get()));
        applicationContext->clientStateHandler()->registerDelegate(
            kWindowGeometryData,
            std::move(geometryManager));
    }

    // We must handle all 'WindowScreenChange' events _before_ we move window.
    qApp->processEvents();

    applicationContext->clientStateHandler()->clientStarted(
        StartupParameters::fromCommandLineParams(startupParams));

    #if defined(Q_OS_WIN)
        if (qnRuntime->isVideoWallMode() && !ini().showBorderInVideoWallMode)
        {
            mainWindow->windowHandle()
                ->nativeInterface<QNativeInterface::Private::QWindowsWindow>()
                ->setHasBorderInFullScreen(true);
        }
    #endif

    mainWindow->show();
    mainWindow->updateDecorationsState();

    client.initDesktopCamera(qobject_cast<QOpenGLWidget*>(mainWindow->viewport()));
    client.startLocalSearchers();

    workbenchContext->handleStartupParameters(startupParams);

    int result = application->exec();

    applicationContext->clientStateHandler()->unregisterDelegate(kWindowGeometryData);

    /* Write out settings. */
    appContext()->localSettings()->audioVolume = nx::audio::AudioDevice::instance()->volume();

    // Wait while deleteLater objects will be freed
    WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed(QThread::currentThread(), 3);
    waitingForObjectsToBeFreed.join();

    return result;
}

int runApplication(int argc, char** argv)
{
    nx::kit::OutputRedirector::ensureOutputRedirection();

    if (build_info::isLinux())
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "xcb");
    }

#ifdef Q_WS_X11
    XInitThreads();
#endif

#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
    win32_exception::installGlobalUnhandledExceptionHandler();
#endif

    // TODO: Qt6 workaround for broken macOS style of QuickControls2.
    // https://forum.qt.io/topic/131823/lots-of-typeerrors-in-console-when-migrating-to-qt6/5
    // https://bugreports.qt.io/browse/QTBUG-98098
    QQuickStyle::setStyle("Basic");
    initQmlGlyphCacheWorkaround();

    nx::utils::rlimit::setMaxFileDescriptors(8000);

    // This attribute is needed to embed QQuickWidget into other QWidgets.
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    const QnStartupParameters startupParams = QnStartupParameters::fromCommandLineArg(argc, argv);
    if (startupParams.hiDpiDisabled)
    {
        QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    }
    else
    {
        // These attributes must be set before application instance is created.
        QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

        if (ini().roundDpiScaling)
        {
            QApplication::setHighDpiScaleFactorRoundingPolicy(
                Qt::HighDpiScaleFactorRoundingPolicy::Round);
        }
    }

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    if (nx::build_info::isMacOsX())
    {
        // This should go into QnClientModule::initSurfaceFormat(),
        // but we must set OpenGL version before creation of GUI application.

        QSurfaceFormat format;
        // Mac computers OpenGL versions:
        //   https://support.apple.com/en-us/HT202823
        format.setProfile(QSurfaceFormat::CoreProfile);
        // Chromium requires OpenGL 4.1 on macOS for WebGL and other HW accelerated staff.
        format.setVersion(4, 1);

        QSurfaceFormat::setDefaultFormat(format);
    }

    auto application = std::make_unique<QApplication>(argc, argv);
    initApplication(startupParams);

    // Initialize speech synthesis.
    const QString applicationDirPath = QCoreApplication::applicationDirPath();
    NX_ASSERT(!applicationDirPath.isEmpty(), "QApplication may not have been initialized.");
    auto textToWaveServer = std::make_unique<nx::speech_synthesizer::TextToWaveServer>(
        applicationDirPath);
    textToWaveServer->start();
    textToWaveServer->waitForStarted();

    // Adding exe dir to plugin search path.
    QStringList pluginDirs = QCoreApplication::libraryPaths();
    pluginDirs << QCoreApplication::applicationDirPath();
    QCoreApplication::setLibraryPaths(pluginDirs);
#ifdef Q_OS_LINUX
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, lit("/etc/xdg"));
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, lit("/etc/xdg"));
#endif


#ifdef Q_OS_MAC
    mac_restoreFileAccess();
#endif

    int result = runApplicationInternal(application.get(), startupParams);

#ifdef Q_OS_MAC
    mac_stopFileAccess();
#endif

    return result;
}

} // namespace nx::vms::client::desktop {
