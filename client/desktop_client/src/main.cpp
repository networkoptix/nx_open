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

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_module.h>
#include <client/client_startup_parameters.h>
#include <client/self_updater.h>

#include <nx/network/app_info.h>
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

#ifndef DISABLE_FESTIVAL
#include <nx_speech_synthesizer/text_to_wav.h>
#include <nx/utils/file_system.h>
#endif

#include <utils/common/app_info.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>

#include <plugins/io_device/joystick/joystick_manager.h>

namespace
{
    const int kSuccessCode = 0;
    const int kInvalidParametersCode = -1;
    static const nx::utils::log::Tag kMainWindow(lit("MainWindow"));
}

#ifndef API_TEST_MAIN

namespace nx {
namespace client {
namespace desktop {

int runApplication(QtSingleApplication* application, const QnStartupParameters& startupParams)
{
    const bool allowMultipleClientInstances = startupParams.allowMultipleClientInstances
        || !startupParams.customUri.isNull()
        || !startupParams.videoWallGuid.isNull();

    if (startupParams.customUri.isValid())
    {
        QPointer<QNetworkAccessManager> manager(new QNetworkAccessManager(application));
        QObject::connect(manager.data(), &QNetworkAccessManager::finished,
            [manager](QNetworkReply* reply)
            {
                qDebug() << lit("Cloud Reply received: %1").arg(QLatin1String(reply->readAll()));
                reply->deleteLater();
                manager->deleteLater();
            });

        QUrl url(nx::network::AppInfo::defaultCloudPortalUrl());
        url.setPath(lit("/api/utils/visitedKey"));
        qDebug() << "Sending Cloud Portal Confirmation to" << url.toString();

        QJsonObject data{{lit("key"), startupParams.customUri.authenticator().encode()}};
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, lit("application/json"));

        manager->post(request, QJsonDocument(data).toJson(QJsonDocument::Compact));
    }

    if (!allowMultipleClientInstances)
    {
        /* Check if application is already running. */
        if (application->isRunning() && application->sendMessage(startupParams.files.join(L'\n')))
            return kSuccessCode;
    }

    QnClientModule client(startupParams);

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

    /* Create main window. */
    Qt::WindowFlags flags = qnRuntime->isVideoWallMode()
        ? Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint
        : static_cast<Qt::WindowFlags>(0);

    // todo: remove it. VMS-5837
    using namespace nx::client::plugins::io_device;
    std::unique_ptr<joystick::Manager> joystickManager(new joystick::Manager(context.data()));

    QScopedPointer<ui::MainWindow> mainWindow(new ui::MainWindow(context.data(), NULL, flags));
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
        NX_INFO(kMainWindow) << "Running application on a custom screen" << startupParams.screen;
        const auto windowHandle = mainWindow->windowHandle();
        if (windowHandle && (desktop->screenCount() > 0))
        {
            /* We must handle all 'WindowScreenChange' events _before_ we changing screen. */
            qApp->processEvents();

            /* Set target screen for fullscreen mode. */
            const auto screen = QGuiApplication::screens().value(startupParams.screen, 0);
            NX_INFO(kMainWindow) << "Target screen geometry is" << screen->geometry();
            windowHandle->setScreen(screen);

            // Set target position for the window when we set fullscreen off.
            QPoint screenDelta = mainWindow->pos()
                - desktop->screenGeometry(mainWindow.data()).topLeft();
            NX_INFO(kMainWindow) << "Current display offset is" << screenDelta;

            QPoint targetPosition = desktop->screenGeometry(startupParams.screen).topLeft()
                + screenDelta;
            NX_INFO(kMainWindow) << "Target top-left corner position is" << targetPosition;

            mainWindow->move(targetPosition);
        }
    }
    bool debugScreensGeometry = startupParams.screen != QnStartupParameters::kInvalidScreen
        && startupParams.screen >= desktop->screenCount();
    if (debugScreensGeometry)
    {
        NX_INFO(kMainWindow) << "System screen configuration:";
        for (int i = 0; i < desktop->screenCount(); ++i)
        {
            const auto screen = QGuiApplication::screens().value(i);
            NX_INFO(kMainWindow) << "Screen" << i << "geometry is" << screen->geometry();
            if (desktop->screenGeometry(i) != screen->geometry())
                NX_INFO(kMainWindow) << "Alternative screen" << i << "geometry is" << desktop->screenGeometry(i);
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

    if (!context->handleStartupParameters(startupParams))
        return kInvalidParametersCode;  /* For now it is only if starting videowall failed. */

    int result = application->exec();

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

#ifndef DISABLE_FESTIVAL
    std::unique_ptr<TextToWaveServer> textToWaveServer = std::make_unique<TextToWaveServer>(
        nx::utils::file_system::applicationDirPath(argc, argv));

    textToWaveServer->start();
    textToWaveServer->waitForStarted();
#endif

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
