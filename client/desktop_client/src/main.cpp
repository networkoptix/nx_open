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

#include <client/self_updater.h>

#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QScopedPointer>

#include <QtGui/QDesktopServices>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <QtSingleApplication>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_module.h>

#include <client/client_startup_parameters.h>

#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>

#include <nx/audio/audiodevice.h>
#include <nx/utils/crash_dump/systemexcept.h>

#include <ui/actions/action_manager.h>
#include <ui/help/help_handler.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#ifdef Q_OS_MAC
#include <ui/workaround/mac_utils.h>
#endif

#include <utils/common/app_info.h>
#include <utils/common/util.h>
#include <utils/common/command_line_parser.h>

namespace
{
    const int kSuccessCode = 0;
    const int kInvalidParametersCode = -1;
}

#ifndef API_TEST_MAIN

int runApplication(QtSingleApplication* application, int argc, char **argv)
{
    const QnStartupParameters startupParams = QnStartupParameters::fromCommandLineArg(argc, argv);

    const bool allowMultipleClientInstances = startupParams.allowMultipleClientInstances
        || !startupParams.customUri.isNull()
        || !startupParams.videoWallGuid.isNull();

    if (startupParams.customUri.isValid())
    {
        QUrl url(QnAppInfo::defaultCloudPortalUrl());
        url.setPath(lit("/api/utils/visitedKey"));

        QJsonObject data{{lit("key"), startupParams.customUri.authenticator().encode()}};

        QNetworkAccessManager manager;
        manager.post(QNetworkRequest(url), QJsonDocument(data).toJson(QJsonDocument::Compact));
    }


    if (!allowMultipleClientInstances)
    {
        QString argsMessage;
        for (int i = 1; i < argc; ++i)
            argsMessage += fromNativePath(QFile::decodeName(argv[i])) + QLatin1Char('\n');

        /* Check if application is already running. */
        if (application->isRunning() && application->sendMessage(argsMessage))
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

    /* Create main window. */
    Qt::WindowFlags flags = qnRuntime->isVideoWallMode()
        ? Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint
        : static_cast<Qt::WindowFlags>(0);
    QScopedPointer<QnMainWindow> mainWindow(new QnMainWindow(context.data(), NULL, flags));
    context->setMainWindow(mainWindow.data());
    mainWindow->setAttribute(Qt::WA_QuitOnClose);
    application->setActivationWindow(mainWindow.data());

    QDesktopWidget *desktop = qApp->desktop();
    bool customScreen = startupParams.screen != QnStartupParameters::kInvalidScreen && startupParams.screen < desktop->screenCount();
    if (customScreen)
    {
        const auto windowHandle = mainWindow->windowHandle();
        if (windowHandle && (desktop->screenCount() > 0))
        {
            /* We must handle all 'WindowScreenChange' events _before_ we changing screen. */
            qApp->processEvents();

            /* Set target screen for fullscreen mode. */
            windowHandle->setScreen(QGuiApplication::screens().value(startupParams.screen, 0));

            /* Set target position for the window when we set fullscreen off. */
            QPoint screenDelta = mainWindow->pos() - desktop->screenGeometry(mainWindow.data()).topLeft();
            QPoint targetPosition = desktop->screenGeometry(startupParams.screen).topLeft() + screenDelta;
            mainWindow->move(targetPosition);
        }
    }
    mainWindow->show();
    if (customScreen)
    {
        /* We must handle 'move' event _before_ we activate fullscreen. */
        qApp->processEvents();
    }

    const bool instantlyMaximize = !startupParams.fullScreenDisabled && qnRuntime->isDesktopMode();

    if (instantlyMaximize)
        context->action(QnActions::EffectiveMaximizeAction)->trigger();
    else
        mainWindow->updateDecorationsState();

    if (!allowMultipleClientInstances)
        QObject::connect(application, &QtSingleApplication::messageReceived, mainWindow.data(), &QnMainWindow::handleMessage);

    client.initDesktopCamera(dynamic_cast<QGLWidget*>(mainWindow->viewport()));
    client.startLocalSearchers();

    if (!context->handleStartupParameters(startupParams))
        return kInvalidParametersCode;  /* For now it is only if starting videowall failed. */

    int result = application->exec();

    /* Write out settings. */
    qnSettings->setAudioVolume(nx::audio::AudioDevice::instance()->volume());
    qnSettings->save();

    return result;
}

int main(int argc, char **argv)
{
#ifdef Q_WS_X11
    XInitThreads();
#endif

#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
    win32_exception::installGlobalUnhandledExceptionHandler();
#endif

#ifdef Q_OS_LINUX
    linux_exception::installCrashSignalHandler();
#endif

#ifdef Q_OS_MAC
    mac_setLimits();
#endif

    /* These attributes must be set before application instance is created. */
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
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

    int result = runApplication(application.data(), argc, argv);

#ifdef Q_OS_MAC
    mac_stopFileAccess();
#endif

    return result;
}

#endif // API_TEST_MAIN
