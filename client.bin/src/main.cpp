//#define QN_USE_VLD
#include <cstdint>

#ifdef QN_USE_VLD
#   include <vld.h>
#endif

#include <qglobal.h>

#ifdef Q_OS_WIN
#include <common/systemexcept_win32.h>
#endif
#ifdef Q_OS_LINUX
#include <common/systemexcept_linux.h>
#endif

#ifdef Q_OS_LINUX
#   include <unistd.h>
#endif

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

#include <iostream>

#include "self_updater.h"

#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtGui/QDesktopServices>
#include <QtSingleApplication>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_module.h>

#include <client/client_startup_parameters.h>

#include <nx_speach_synthesizer/text_to_wav.h>

#include <nx/vms/utils/system_uri.h>
#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>

#include <openal/qtvaudiodevice.h>

#include <ui/actions/action_manager.h>
#include <ui/help/help_handler.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>

#ifdef Q_OS_MAC
#include <ui/workaround/mac_utils.h>
#endif
#ifdef Q_OS_LINUX
#include <ui/workaround/x11_launcher_workaround.h>
#endif

#include <utils/common/app_info.h>
#include <utils/common/util.h>
#include <utils/common/command_line_parser.h>

#include <watchers/cloud_status_watcher.h>

namespace
{
    const int kSuccessCode = 0;
}

static QtMessageHandler defaultMsgHandler = 0;

static void myMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (defaultMsgHandler)
    {
        defaultMsgHandler(type, ctx, msg);
    }
    else
    { /* Default message handler. */
#ifndef QN_NO_STDERR_MESSAGE_OUTPUT
        QTextStream err(stderr);
        err << msg << endl << flush;
#endif
    }

    qnLogMsgHandler(type, ctx, msg);
}

#ifndef API_TEST_MAIN

int runApplication(QtSingleApplication* application, int argc, char **argv)
{
    const QnStartupParameters startupParams = QnStartupParameters::fromCommandLineArg(argc, argv);
    bool allowMultipleClientInstances = startupParams.allowMultipleClientInstances;
    bool fullScreenDisabled = startupParams.fullScreenDisabled;

#ifdef Q_OS_WIN
    nx::vms::client::SelfUpdater updater(startupParams);

    /* Immediately exit if run under administrator. */
    if (startupParams.hasAdminPermissions)
        return kSuccessCode;

#endif // Q_OS_WIN

    if (!startupParams.customUri.isNull())
    {
        allowMultipleClientInstances = true;
    }
    else if (!startupParams.videoWallGuid.isNull())
    {
        allowMultipleClientInstances = true;
        fullScreenDisabled = true;
    }

    if (!allowMultipleClientInstances)
    {
        QString argsMessage;
        for (int i = 1; i < argc; ++i)
            argsMessage += fromNativePath(QFile::decodeName(argv[i])) + QLatin1Char('\n');

        if (application->isRunning())
        {
            if (application->sendMessage(argsMessage))
                return kSuccessCode;
        }
    }

    QnClientModule client(startupParams);

    /* This class should not be initialized in client module as it does not support deinitialization. */
    QScopedPointer<TextToWaveServer> textToWaveServer(new TextToWaveServer());
    textToWaveServer->start();

    /* Initialize sound. */
    QtvAudioDevice::instance()->setVolume(qnSettings->audioVolume());

    QnHelpHandler helpHandler;
    qApp->installEventFilter(&helpHandler);

    cl_log.log(qApp->applicationDisplayName(), " started", cl_logALWAYS);
    cl_log.log("Software version: ", QApplication::applicationVersion(), cl_logALWAYS);
    cl_log.log("binary path: ", qApp->applicationFilePath(), cl_logALWAYS);

    defaultMsgHandler = qInstallMessageHandler(myMsgHandler);


    /* Create workbench context. */
    QScopedPointer<QnWorkbenchContext> context(new QnWorkbenchContext());

    QnActions::IDType effectiveMaximizeActionId = QnActions::FullscreenAction;
#ifdef Q_OS_LINUX
    /* In Ubuntu its launcher is configured to be shown when a non-fullscreen window has appeared.
     * In our case it means that launcher overlaps our fullscreen window when the user opens any dialogs.
     * To prevent such overlapping there was an attempt to hide unity launcher when the main window
     * has been activated. But now we can't hide launcher window because there is no any visible window for it.
     * Unity-3D launcher is like a 3D-effect activated by compiz window manager.
     * We can investigate possibilities of changing the behavior of unity compiz plugin but now
     * we just disable fullscreen for unity-3d desktop session.
     */
    if (QnX11LauncherWorkaround::isUnity3DSession())
        effectiveMaximizeActionId = QnActions::MaximizeAction;
#endif
    context->menu()->registerAlias(QnActions::EffectiveMaximizeAction, effectiveMaximizeActionId);

    /* Create main window. */
    Qt::WindowFlags flags = qnRuntime->isVideoWallMode()
        ? Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint
        : static_cast<Qt::WindowFlags>(0);
    QScopedPointer<QnMainWindow> mainWindow(new QnMainWindow(context.data(), NULL, flags));
    context->setMainWindow(mainWindow.data());
    mainWindow->setAttribute(Qt::WA_QuitOnClose);
    application->setActivationWindow(mainWindow.data());

    bool customScreen = startupParams.screen != QnStartupParameters::kInvalidScreen;
    if (customScreen)
    {
        QDesktopWidget *desktop = qApp->desktop();
        if (startupParams.screen >= 0 && startupParams.screen < desktop->screenCount())
        {
            QPoint screenDelta = mainWindow->pos() - desktop->screenGeometry(mainWindow.data()).topLeft();
            QPoint targetPosition = desktop->screenGeometry(startupParams.screen).topLeft() + screenDelta;
            mainWindow->move(targetPosition);
        }
    }

    mainWindow->show();
    if (customScreen)
        qApp->processEvents(); /* We must handle 'move' event _before_ we activate fullscreen. */

    if (!fullScreenDisabled)
    {
        context->action(QnActions::EffectiveMaximizeAction)->trigger();
    }
    else
    {
        mainWindow->updateDecorationsState();
    }

    if (!allowMultipleClientInstances)
        QObject::connect(application, &QtSingleApplication::messageReceived, mainWindow.data(), &QnMainWindow::handleMessage);

    client.initDesktopCamera(dynamic_cast<QGLWidget*>(mainWindow->viewport()));

    /* Process input files. */
    bool haveInputFiles = false;
    {
        bool skipArg = true;
        for (const auto& arg : qApp->arguments())
        {
            if (!skipArg)
                haveInputFiles |= mainWindow->handleMessage(arg);
            skipArg = false;
        }
    }

    if (startupParams.customUri.isValid())
    {
        using namespace nx::vms::utils;
        switch (startupParams.customUri.clientCommand())
        {
            case SystemUri::ClientCommand::LoginToCloud:
            {
                SystemUri::Auth auth = startupParams.customUri.authenticator();
                qnCommon->instance<QnCloudStatusWatcher>()->setCloudCredentials(auth.user, auth.password, true);
                break;
            }
            case SystemUri::ClientCommand::ConnectToSystem:
            {
                SystemUri::Auth auth = startupParams.customUri.authenticator();
                QString systemId = startupParams.customUri.systemId();
                bool systemIsCloud = !QnUuid::fromStringSafe(systemId).isNull();

                QUrl systemUrl = QUrl::fromUserInput(systemId);
                systemUrl.setUserName(auth.user);
                systemUrl.setPassword(auth.password);

                if (systemIsCloud)
                    qnCommon->instance<QnCloudStatusWatcher>()->setCloudCredentials(auth.user, auth.password, true);

                context->menu()->trigger(QnActions::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, systemUrl));
                break;
            }
            default:
                break;
        }
    }
    /* If no input files were supplied --- open connection settings dialog.
     * Do not try to connect in the following cases:
     * * we were not connected and clicked "Open in new window"
     * * we have opened exported exe-file
     * Otherwise we should try to connect or show Login Dialog.
     */
    else if (startupParams.instantDrop.isEmpty() && !haveInputFiles)
    {
/* Set authentication parameters from command line. */
        QUrl appServerUrl = QUrl::fromUserInput(startupParams.authenticationString); //TODO: #refactor System URI to support videowall
        if (!startupParams.videoWallGuid.isNull())
        {
            NX_ASSERT(appServerUrl.isValid());
            if (!appServerUrl.isValid())
            {
                return -1;
            }
            appServerUrl.setUserName(startupParams.videoWallGuid.toString());
        }
        context->menu()->trigger(QnActions::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, appServerUrl));
    }

    if (!startupParams.videoWallGuid.isNull())
    {
        context->menu()->trigger(QnActions::DelayedOpenVideoWallItemAction, QnActionParameters()
                                 .withArgument(Qn::VideoWallGuidRole, startupParams.videoWallGuid)
                                 .withArgument(Qn::VideoWallItemGuidRole, startupParams.videoWallItemGuid));
    }
    else if (!startupParams.delayedDrop.isEmpty())
    { /* Drop resources if needed. */
        NX_ASSERT(startupParams.instantDrop.isEmpty());

        QByteArray data = QByteArray::fromBase64(startupParams.delayedDrop.toLatin1());
        context->menu()->trigger(QnActions::DelayedDropResourcesAction, QnActionParameters().withArgument(Qn::SerializedDataRole, data));
    }
    else if (!startupParams.instantDrop.isEmpty())
    {
        QByteArray data = QByteArray::fromBase64(startupParams.instantDrop.toLatin1());
        context->menu()->trigger(QnActions::InstantDropResourcesAction, QnActionParameters().withArgument(Qn::SerializedDataRole, data));
    }

    // show beta version warning message for the main instance only
    if (!allowMultipleClientInstances &&
        !qnRuntime->isDevMode() &&
        QnAppInfo::beta())
        context->action(QnActions::BetaVersionMessageAction)->trigger();

#ifdef _DEBUG
    /* Show FPS in debug. */
    context->menu()->trigger(QnActions::ShowFpsAction);
#endif

    int result = application->exec();

    /* Write out settings. */
    qnSettings->setAudioVolume(QtvAudioDevice::instance()->volume());
    qnSettings->save();

    //restoring default message handler
    qInstallMessageHandler(defaultMsgHandler);

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
