// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application.h"

#include <qglobal.h>

#ifdef Q_OS_LINUX
#   include <unistd.h>
#endif

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

#include <iostream>

#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QScopedPointer>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QSettings>
#include <QtCore/QFile>
#include <QtCore/QDateTime>

#include <QtGui/QDesktopServices>
#include <QtGui/QWindow>
#include <QtGui/QScreen>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QOpenGLWidget>

#include <QtWebEngine/QtWebEngine>

#include <nx/kit/output_redirector.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_module.h>
#include <client/client_startup_parameters.h>
#include <client/self_updater.h>

#include <client_core/client_core_module.h>

#include <nx/media/decoder_registrar.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/rlimit.h>

#include <nx/audio/audiodevice.h>
#include <nx/utils/crash_dump/systemexcept.h>

#include <common/common_module.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/joystick/joystick_settings_action_handler.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/state/window_controller.h>
#include <nx/vms/client/desktop/state/window_geometry_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/eula_dialog.h>
#include <statistics/statistics_manager.h>
#include <ui/graphics/instruments/gl_checker_instrument.h>
#include <ui/help/help_handler.h>
#include <ui/statistics/modules/session_restore_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#if defined(Q_OS_MACOS)
    #include <sys/sysctl.h>
    #include <ui/workaround/mac_utils.h>
#endif

#include <ui/workaround/combobox_wheel_filter.h>

#include <nx/speech_synthesizer/text_to_wave_server.h>

#include <utils/common/command_line_parser.h>
#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>

#include <nx/vms/client/desktop/ini.h>
#if defined(Q_OS_WIN)
    #include <nx/gdi_tracer/gdi_handle_tracer.h>
    #include <nx/vms/client/desktop/debug_utils/instruments/widget_profiler.h>
    #include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif

#include <nx/branding.h>
#include <nx/build_info.h>

namespace {

const int kSuccessCode = 0;
const int kInvalidParametersCode = -1;
static const nx::utils::log::Tag kMainWindow(lit("MainWindow"));

const QString kWindowGeometryData = "windowGeometry";

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
        url.setPath(lit("/api/utils/visitedKey"));

        const QJsonObject data{{lit("key"), uri.authenticator().encode()}};
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, lit("application/json"));

        manager->post(request, QJsonDocument(data).toJson(QJsonDocument::Compact));
}

Qt::WindowFlags calculateWindowFlags()
{
    if (qnRuntime->isAcsMode())
        return Qt::Window;

    Qt::WindowFlags result = nx::build_info::isMacOsX()
        ? Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint
        : Qt::Window | Qt::CustomizeWindowHint;

    if (qnRuntime->isVideoWallMode())
        result |= Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint;

    return result;
}

static void initQmlGlyphCacheWorkaround()
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

namespace nx::vms::client::desktop {

int runApplicationInternal(QApplication* application, const QnStartupParameters& startupParams)
{
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
        nx::vms::client::SelfUpdater updater(startupParams);
    }

    /* Immediately exit if run in self-update mode. */
    if (startupParams.selfUpdateMode)
        return kSuccessCode;

    /* Initialize sound. */
    nx::audio::AudioDevice::instance()->setVolume(qnSettings->audioVolume());

    qApp->installEventFilter(&QnHelpHandler::instance());

    // Hovered QComboBox changes its value when user scrolls a mouse wheel, even if the ComboBox
    // is not focused. It leads to weird and undesirable UI behaviour in some parts of the client.
    // We use a global Event Filter to prevent QComboBox instances from receiving Wheel events.
    ComboboxWheelFilter wheelFilter;
    qApp->installEventFilter(&wheelFilter);

    /* Create workbench context. */
    QScopedPointer<QnWorkbenchAccessController> accessController(
        new QnWorkbenchAccessController(client.clientCoreModule()->commonModule()));
    QScopedPointer<QnWorkbenchContext> context(new QnWorkbenchContext(accessController.data()));

    #if defined(Q_OS_LINUX)
        qputenv("RESOURCE_NAME", nx::branding::brand().toUtf8());
    #endif

    // Dealing with EULA in videowall mode can make people frown.
    if (!qnRuntime->isVideoWallMode() && qnRuntime->isDesktopMode())
    {
        int accepted = qnSettings->acceptedEulaVersion();
        int current = nx::branding::eulaVersion();
        const bool showEula = accepted < current;
        if (showEula
            && !EulaDialog::acceptEulaFromFile(":/license.html", current, context->mainWindow()))
        {
            // We should exit completely.
            return 0;
        }
    }

    /* Create main window. */

    QScopedPointer<MainWindow> mainWindow(
        new MainWindow(context.data(), nullptr, calculateWindowFlags()));
    context->setMainWindow(mainWindow.data());
    mainWindow->setAttribute(Qt::WA_QuitOnClose);

    std::unique_ptr<nx::vms::client::desktop::joystick::JoystickSettingsActionHandler>
        hidJoystickManager(
            new nx::vms::client::desktop::joystick::JoystickSettingsActionHandler(context.get()));

    #if defined(Q_OS_LINUX)
        qunsetenv("RESOURCE_NAME");
    #endif

    nx::media::DecoderRegistrar::registerDecoders({}, true);

    const auto desktop = qApp->desktop();
    bool customScreen = startupParams.screen != QnStartupParameters::kInvalidScreen
        && startupParams.screen < QGuiApplication::screens().size();

    // Window handle must exist before events processing. This is required to initialize the scene.
    volatile auto winId = mainWindow->winId();

    auto geometryManager = std::make_unique<WindowGeometryManager>(
        std::make_unique<WindowController>(mainWindow.get()));
    client.clientStateHandler()->registerDelegate(kWindowGeometryData, std::move(geometryManager));
    client.clientStateHandler()->setStatisticsModules(
        context->commonModule()->instance<QnStatisticsManager>(),
        context->instance<QnSessionRestoreStatisticsModule>());

    // We must handle all 'WindowScreenChange' events _before_ we move window.
    qApp->processEvents();

    if (customScreen)
    {
        NX_INFO(kMainWindow) << "Running application on a custom screen" << startupParams.screen;
        const auto windowHandle = mainWindow->windowHandle();
        if (windowHandle && QGuiApplication::screens().size() > 0)
        {
            // Set target screen for fullscreen mode.
            const auto screen = QGuiApplication::screens().value(startupParams.screen, 0);
            NX_INFO(kMainWindow) << "Target screen geometry is" << screen->geometry();
            windowHandle->setScreen(screen);

            // Set target position for the window when we set fullscreen off.
            QPoint screenDelta = mainWindow->pos()
                - desktop->screenGeometry(mainWindow.data()).topLeft();
            NX_INFO(kMainWindow) << "Current display offset is" << screenDelta;

            QPoint targetPosition = screen->geometry().topLeft() + screenDelta;
            NX_INFO(kMainWindow) << "Target top-left corner position is" << targetPosition;

            mainWindow->move(targetPosition);
        }
    }
    else
    {
        client.clientStateHandler()->clientStarted(
            StartupParameters::fromCommandLineParams(startupParams));
    }

    #if defined(Q_OS_WIN)
        if (qnRuntime->isVideoWallMode())
            QWindowsWindowFunctions::setHasBorderInFullScreen(mainWindow->windowHandle(), true);
    #endif

    mainWindow->show();

    const bool instantlyMaximize = false
        && !startupParams.fullScreenDisabled
//        && !customWindowGeometry
        && qnRuntime->isDesktopMode();

    if (instantlyMaximize)
    {
        // We must handle 'move' event _before_ we activate fullscreen.
        qApp->processEvents();
        context->menu()->trigger(ui::action::EffectiveMaximizeAction);
    }
    else
    {
        mainWindow->updateDecorationsState();
    }

    client.initDesktopCamera(qobject_cast<QOpenGLWidget*>(mainWindow->viewport()));
    client.startLocalSearchers();

    context->handleStartupParameters(startupParams);

    #if defined(Q_OS_WIN)
        if (ini().enableGdiTrace)
        {
            const auto traceLimitCallback =
                []()
                {
                    const auto reportFileName = lit("/log/gdi_handles_report_%1.txt")
                        .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy_hh.mm.ss"));

                    const auto reportPathString =
                        QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                        + reportFileName;

                    QFile reportFile(reportPathString);
                    if (reportFile.open(QFile::WriteOnly | QFile::Truncate))
                    {
                        const auto gdiTracer = gdi_tracer::GdiHandleTracer::getInstance();
                        QTextStream textStream(&reportFile);
                        textStream.setCodec("UTF-8");
                        textStream << QString::fromStdString(gdiTracer->getReport());
                        textStream << WidgetProfiler::getWidgetHierarchy();
                    }
                };

            auto gdiTracer = gdi_tracer::GdiHandleTracer::getInstance();
            gdiTracer->setGdiTraceLimit(ini().gdiTraceLimit);
            gdiTracer->setGdiTraceLimitCallback(traceLimitCallback);
            gdiTracer->attachGdiDetours();
        }
    #endif

    int result = application->exec();

    client.clientStateHandler()->unregisterDelegate(kWindowGeometryData);

    #if defined(Q_OS_WIN)
        if (ini().enableGdiTrace)
            gdi_tracer::GdiHandleTracer::getInstance()->detachGdiDetours();
    #endif

    /* Write out settings. */
    qnSettings->setAudioVolume(nx::audio::AudioDevice::instance()->volume());
    qnSettings->save();

    // Wait while deleteLater objects will be freed
    WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed(QThread::currentThread(), 3);
    waitingForObjectsToBeFreed.join();

    return result;
}

int runApplication(int argc, char** argv)
{
    nx::kit::OutputRedirector::ensureOutputRedirection();

#ifdef Q_WS_X11
    XInitThreads();
#endif

#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
    win32_exception::installGlobalUnhandledExceptionHandler();
#endif

    initQmlGlyphCacheWorkaround();

    nx::utils::rlimit::setMaxFileDescriptors(8000);

    // This attribute is needed to embed QQuickWidget into other QWidgets.
    //QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

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
    }

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

    QtWebEngine::initialize();

    auto application = std::make_unique<QApplication>(argc, argv);

    // Initialize speech synthesis.
    const QString applicationDirPath = QCoreApplication::applicationDirPath();
    NX_ASSERT(!applicationDirPath.isEmpty(), "QApplication may not have been initialized.");
    auto textToWaveServer = std::make_unique<nx::speech_synthesizer::TextToWaveServer>(
        applicationDirPath);
    textToWaveServer->start();
    textToWaveServer->waitForStarted();

     // This is necessary to prevent crashes when we want to use QDesktopWidget from the non-main
    // thread before any window has been created.
    qApp->desktop();

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
