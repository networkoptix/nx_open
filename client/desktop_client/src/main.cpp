//#define QN_USE_VLD
#include <cstdint>

#ifdef QN_USE_VLD
#   include <vld.h>
#endif

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

#include <QtGui/QDesktopServices>
#include <QtGui/QWindow>
#include <QtGui/QScreen>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include <QtOpenGL/QGLWidget>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <QtSingleApplication>

#include <common/static_common_module.h>

#include <client/client_app_info.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_module.h>
#include <client/client_startup_parameters.h>
#include <client/self_updater.h>

#include <nx/network/app_info.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>

#include <nx/audio/audiodevice.h>
#include <nx/utils/crash_dump/systemexcept.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/help/help_handler.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#ifdef Q_OS_MAC
#include <ui/workaround/mac_utils.h>
#endif

#include <nx_speech_synthesizer/text_to_wav.h>
#include <nx/utils/file_system.h>

#include <utils/common/app_info.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>

#include <plugins/io_device/joystick/joystick_manager.h>

#if defined(Q_OS_WIN64)
    #include <ini.h>
    #include <nx/gdi_tracer/gdi_handle_tracer.h>
#endif

namespace {

const int kSuccessCode = 0;
const int kInvalidParametersCode = -1;

void sendCloudPortalConfirmation(const nx::vms::utils::SystemUri& uri, QObject* owner)
{
    QPointer<QNetworkAccessManager> manager(new QNetworkAccessManager(owner));
        QObject::connect(manager.data(), &QNetworkAccessManager::finished,
            [manager](QNetworkReply* reply)
            {
                reply->deleteLater();
                manager->deleteLater();
            });

        QUrl url(nx::network::AppInfo::defaultCloudPortalUrl(
            nx::network::SocketGlobals::cloudHost()));
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

    Qt::WindowFlags result = nx::utils::AppInfo::isMacOsX()
        ? Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint
        : Qt::Window | Qt::CustomizeWindowHint;

    if (qnRuntime->isVideoWallMode())
        result |= Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint;

    return result;
}

} // namespace

#ifndef API_TEST_MAIN

namespace nx {
namespace client {
namespace desktop {

int runApplication(QtSingleApplication* application, const QnStartupParameters& startupParams)
{
    const bool allowMultipleClientInstances = startupParams.allowMultipleClientInstances
        || !startupParams.customUri.isNull()
        || !startupParams.videoWallGuid.isNull();

    if (!allowMultipleClientInstances)
    {
        /* Check if application is already running. */
        if (application->isRunning() && application->sendMessage(startupParams.files.join(L'\n')))
            return kSuccessCode;
    }

    QnClientModule client(startupParams);

    if (startupParams.customUri.isValid())
        sendCloudPortalConfirmation(startupParams.customUri, application);

    /* Running updater after QApplication and NX_LOG are initialized. */
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

    QnHelpHandler helpHandler;
    qApp->installEventFilter(&helpHandler);

    /* Create workbench context. */
    QScopedPointer<QnWorkbenchAccessController> accessController(new QnWorkbenchAccessController());
    QScopedPointer<QnWorkbenchContext> context(new QnWorkbenchContext(accessController.data()));

    #if defined(Q_OS_LINUX)
        qputenv("RESOURCE_NAME", QnAppInfo::productNameShort().toUtf8());
    #endif

    // Dealing with EULA in videowall mode can make people frown.
    if (!qnRuntime->isVideoWallMode() && qnRuntime->isDesktopMode())
    {
        int accepted = qnSettings->acceptedEulaVersion();
        int current = QnClientAppInfo::eulaVersion();
        const bool showEula =  accepted < current;
        if (showEula && !context->showEulaMessage())
        {
            // We should exit completely.
            return 0;
        }
    }

    /* Create main window. */

    // todo: remove it. VMS-5837
    using namespace nx::client::plugins::io_device;
    std::unique_ptr<joystick::Manager> joystickManager(new joystick::Manager(context.data()));

    QScopedPointer<ui::MainWindow> mainWindow(
        new ui::MainWindow(context.data(), nullptr, calculateWindowFlags()));
    context->setMainWindow(mainWindow.data());
    mainWindow->setAttribute(Qt::WA_QuitOnClose);
    application->setActivationWindow(mainWindow.data());

    #if defined(Q_OS_LINUX)
        qunsetenv("RESOURCE_NAME");
    #endif

    QDesktopWidget *desktop = qApp->desktop();
    bool customScreen = startupParams.screen != QnStartupParameters::kInvalidScreen
        && startupParams.screen < desktop->screenCount();
    if (customScreen)
    {
        NX_INFO("MainWindow") "Running application on a custom screen" << startupParams.screen;
        const auto windowHandle = mainWindow->windowHandle();
        if (windowHandle && (desktop->screenCount() > 0))
        {
            /* We must handle all 'WindowScreenChange' events _before_ we changing screen. */
            qApp->processEvents();

            /* Set target screen for fullscreen mode. */
            const auto screen = QGuiApplication::screens().value(startupParams.screen, 0);
            NX_INFO("MainWindow") "Target screen geometry is" << screen->geometry();
            windowHandle->setScreen(screen);

            // Set target position for the window when we set fullscreen off.
            QPoint screenDelta = mainWindow->pos()
                - desktop->screenGeometry(mainWindow.data()).topLeft();
            NX_INFO("MainWindow") "Current display offset is" << screenDelta;

            QPoint targetPosition = desktop->screenGeometry(startupParams.screen).topLeft()
                + screenDelta;
            NX_INFO("MainWindow") "Target top-left corner position is" << targetPosition;

            mainWindow->move(targetPosition);
        }
    }

    mainWindow->show();
    joystickManager->start();
    if (customScreen)
    {
        /* We must handle 'move' event _before_ we activate fullscreen. */
        qApp->processEvents();
    }

    const bool instantlyMaximize = !startupParams.fullScreenDisabled && qnRuntime->isDesktopMode();

    if (instantlyMaximize)
        context->action(ui::action::EffectiveMaximizeAction)->trigger();
    else
        mainWindow->updateDecorationsState();

    if (!allowMultipleClientInstances)
    {
        QObject::connect(application, &QtSingleApplication::messageReceived, mainWindow.data(),
            &ui::MainWindow::handleMessage);
    }

    client.initDesktopCamera(dynamic_cast<QGLWidget*>(mainWindow->viewport()));
    client.startLocalSearchers();

    const auto code = context->handleStartupParameters(startupParams);
    if (code != QnWorkbenchContext::success)
        return code == QnWorkbenchContext::forcedExit ? kSuccessCode : kInvalidParametersCode;

    #if defined(Q_OS_WIN64)
        if (ini().enableGdiTrace)
        {
            const auto reportPathString =
                QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                    + lit("/log/gdi_handles_report%1.txt").arg(QnUuid::createUuid().toString());
            const auto reportPath = std::filesystem::path(reportPathString.toStdWString());
            auto gdiTracer = gdi_tracer::GdiHandleTracer::getInstance();
            gdiTracer->setGdiTraceLimit(ini().gdiTraceLimit);
            gdiTracer->setReportPath(reportPath);
            gdiTracer->attachGdiDetours();
        }
    #endif

    int result = application->exec();

    #if defined(Q_OS_WIN64)
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

} // namespace desktop
} // namespace client
} // namespace nx

int main(int argc, char** argv)
{
#ifdef Q_WS_X11
    XInitThreads();
#endif

#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
    win32_exception::installGlobalUnhandledExceptionHandler();
#endif

#ifdef Q_OS_MAC
    mac_setLimits();
#endif

    std::unique_ptr<TextToWaveServer> textToWaveServer = std::make_unique<TextToWaveServer>(
        nx::utils::file_system::applicationDirPath(argc, argv));

    textToWaveServer->start();
    textToWaveServer->waitForStarted();

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
    QScopedPointer<QtSingleApplication> application(new QtSingleApplication(argc, argv));

    // this is necessary to prevent crashes when we want use QDesktopWidget from the non-main thread before any window has been created
    qApp->desktop();

    //adding exe dir to plugin search path
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

    int result = nx::client::desktop::runApplication(application.data(), startupParams);

#ifdef Q_OS_MAC
    mac_stopFileAccess();
#endif

    return result;
}

#endif // API_TEST_MAIN
