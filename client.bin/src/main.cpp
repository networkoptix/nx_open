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

#include <utils/common/app_info.h>
#include "ui/widgets/main_window.h"
#include <api/global_settings.h>


#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QTranslator>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtGui/QDesktopServices>
#include <QScopedPointer>
#include <QtSingleApplication>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_module.h>
#include <client/client_connection_data.h>
#include <client/client_resource_processor.h>
#include <client/client_startup_parameters.h>


#include "core/resource/media_server_resource.h"
#include "core/resource/storage_resource.h"
#include "core/resource/resource_directory_browser.h"
#include <core/resource_management/resource_pool.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/client_camera_factory.h>

#include "decoders/video/ipp_h264_decoder.h"

#include <nx/utils/log/log.h>
#include <utils/common/command_line_parser.h>

#include "ui/workbench/workbench_context.h"
#include "ui/actions/action_manager.h"

#include "decoders/video/abstract_video_decoder.h"
#ifdef Q_OS_WIN
#include <plugins/resource/desktop_win/desktop_resource_searcher.h>
#endif
#include "utils/common/util.h"
#include "plugins/resource/avi/avi_resource.h"
#include "core/resource_management/resource_discovery_manager.h"

#include "api/app_server_connection.h"

#include "api/session_manager.h"
#include "ui/actions/action_manager.h"
#include <nx/network/socket.h>


#include "plugins/storage/file_storage/qtfile_storage_resource.h"
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "core/resource/camera_history.h"

#ifdef Q_OS_LINUX
#include "ui/workaround/x11_launcher_workaround.h"
#include "common/systemexcept_linux.h"
#endif
#include "utils/common/cryptographic_hash.h"
#include "ui/style/globals.h"
#include "openal/qtvaudiodevice.h"
#include "ui/workaround/fglrx_full_screen.h"
#include "ui/workaround/qtbug_workaround.h"


#ifdef Q_OS_WIN
#include "ui/workaround/iexplore_url_handler.h"
#include "common/systemexcept_win32.h"
#endif

#include "ui/help/help_handler.h"

#include "utils/common/long_runnable.h"

#include "common/common_module.h"
#include "ui/customization/customizer.h"
#include <nx_ec/ec2_lib.h>

#include <network/module_finder.h>
#include <finders/systems_finder.h>
#include <finders/direct_systems_finder.h>
#include <finders/cloud_systems_finder.h>
#include <network/router.h>
#include <api/network_proxy_factory.h>
#include <utils/server_interface_watcher.h>
#include <nx/vms/utils/system_uri.h>

#ifdef Q_OS_MAC
#include "ui/workaround/mac_utils.h"
#endif
#include "api/runtime_info_manager.h"


#include <nx/utils/timer_manager.h>
#include <nx/utils/platform/protocol_handler.h>
#include <nx/vms/utils/app_info.h>

namespace
{
    const int kSuccessCode = 0;
}

void ffmpegInit()
{
    // client uses ordinary QT file to access file system, server uses buffering access implemented inside QnFileStorageResource
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("file"), QnQtFileStorageResource::instance, true);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("qtfile"), QnQtFileStorageResource::instance);
    QnStoragePluginFactory::instance()->registerStoragePlugin(QLatin1String("layout"), QnLayoutFileStorageResource::instance);
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

    auto registerUriHandler = []
    {
        return nx::utils::registerSystemUriProtocolHandler(nx::vms::utils::AppInfo::nativeUriProtocol(),
                                                           qApp->applicationFilePath(),
                                                           nx::vms::utils::AppInfo::nativeUriProtocolDescription());
    };

    /* Register URI handler and instantly exit. */
    if (startupParams.hasAdminPermissions)
    {
        registerUriHandler();
        return kSuccessCode;
    }

    /* Check if uri handler is registered already. */
    if (!registerUriHandler())
    {
        /* Avoid lock-file races. */
        allowMultipleClientInstances = true;

        /* Start another client instance with admin permissions if required. */
        nx::utils::runAsAdministratorWithUAC(qApp->applicationFilePath(),
                                             QStringList()
                                             << QnStartupParameters::kHasAdminPermissionsKey
                                             << QnStartupParameters::kAllowMultipleClientInstancesKey);
    }

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

        while (application->isRunning())
        {
            if (application->sendMessage(argsMessage))
            {
                cl_log.log(lit("Another instance is already running"), cl_logALWAYS);
                return kSuccessCode;
            }
        }
    }

    QnClientModule client(startupParams);




#ifdef Q_OS_WIN
    new QnIexploreUrlHandler(application); /* All effects are placed in the constructor. */
    new QnQtbugWorkaround(application);
#endif

    /* Initialize connections. */
    if (!startupParams.videoWallGuid.isNull())
    {
        QnAppServerConnectionFactory::setVideowallGuid(startupParams.videoWallGuid);
        QnAppServerConnectionFactory::setInstanceGuid(startupParams.videoWallItemGuid);
    }

    std::unique_ptr<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(
        getConnectionFactory(startupParams.videoWallGuid.isNull() ? Qn::PT_DesktopClient : Qn::PT_VideowallClient));
    QnAppServerConnectionFactory::setEC2ConnectionFactory(ec2ConnectionFactory.get());

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = startupParams.videoWallItemGuid.isNull()
        ? Qn::PT_DesktopClient
        : Qn::PT_VideowallClient;
    runtimeData.brand = QnAppInfo::productNameShort();
    runtimeData.videoWallInstanceGuid = startupParams.videoWallItemGuid;
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);    // initializing localInfo

    qnSettings->save();
    if (!QDir(qnSettings->mediaFolder()).exists())
        QDir().mkpath(qnSettings->mediaFolder());

    cl_log.log(QLatin1String("Using ") + qnSettings->mediaFolder() + QLatin1String(" as media root directory"), cl_logALWAYS);

    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());


    /* Initialize sound. */
    QtvAudioDevice::instance()->setVolume(qnSettings->audioVolume());

    QnHelpHandler helpHandler;
    qApp->installEventFilter(&helpHandler);

    cl_log.log(qApp->applicationDisplayName(), " started", cl_logALWAYS);
    cl_log.log("Software version: ", QApplication::applicationVersion(), cl_logALWAYS);
    cl_log.log("binary path: ", QFile::decodeName(argv[0]), cl_logALWAYS);

    defaultMsgHandler = qInstallMessageHandler(myMsgHandler);

    ffmpegInit();

    QScopedPointer<QnModuleFinder> moduleFinder(new QnModuleFinder(true, qnRuntime->isDevMode()));
    moduleFinder->start();

    // TODO: #ynikitenkov: move to common module? -> dependency on moduleFinder

    typedef QScopedPointer<QnAbstractSystemsFinder> SystemsFinderPtr;
    const QScopedPointer<QnSystemsFinder> systemsFinder(new QnSystemsFinder());
    const SystemsFinderPtr directSystemsFinder(new QnDirectSystemsFinder());
    const SystemsFinderPtr cloudSystemsFinder(new QnCloudSystemsFinder());
    systemsFinder->addSystemsFinder(directSystemsFinder.data());
    systemsFinder->addSystemsFinder(cloudSystemsFinder.data());

    QScopedPointer<QnRouter> router(new QnRouter(moduleFinder.data()));

    QScopedPointer<QnServerInterfaceWatcher> serverInterfaceWatcher(new QnServerInterfaceWatcher(router.data()));

    // ===========================================================================

    QnVideoDecoderFactory::setCodecManufacture(QnVideoDecoderFactory::AUTO);

    /* Create workbench context. */
    QScopedPointer<QnWorkbenchContext> context(new QnWorkbenchContext());

    context->instance<QnFglrxFullScreen>(); /* Init fglrx workaround. */

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

    if (startupParams.screen != QnStartupParameters::kInvalidScreen)
    {
        QDesktopWidget *desktop = qApp->desktop();
        if (startupParams.screen >= 0 && startupParams.screen < desktop->screenCount())
        {
            QPoint screenDelta = mainWindow->pos() - desktop->screenGeometry(mainWindow.data()).topLeft();
            mainWindow->move(desktop->screenGeometry(startupParams.screen).topLeft() + screenDelta);
        }
    }

    mainWindow->show();
    if (!fullScreenDisabled)
        context->action(QnActions::EffectiveMaximizeAction)->trigger();
    else
        mainWindow->updateDecorationsState();

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

    if (!allowMultipleClientInstances)
        QObject::connect(application, SIGNAL(messageReceived(const QString &)), mainWindow.data(), SLOT(handleMessage(const QString &)));

    /************************************************************************/
    /* Initializing resource searchers                                      */
    /************************************************************************/
    QnClientResourceProcessor resourceProcessor;
    std::unique_ptr<QnResourceDiscoveryManager> resourceDiscoveryManager(new QnResourceDiscoveryManager());
    resourceProcessor.moveToThread(QnResourceDiscoveryManager::instance());
    QnResourceDiscoveryManager::instance()->setResourceProcessor(&resourceProcessor);

    // ============================
    //QnResourceDirectoryBrowser
    if (!startupParams.skipMediaFolderScan)
    {
        QnResourceDirectoryBrowser::instance().setLocal(true);
        QStringList dirs;
        dirs << qnSettings->mediaFolder();
        dirs << qnSettings->extraMediaFolders();
        QnResourceDirectoryBrowser::instance().setPathCheckList(dirs);
        QnResourceDiscoveryManager::instance()->addDeviceServer(&QnResourceDirectoryBrowser::instance());
    }

    /* Initialize desktop camera searcher. */
#ifdef Q_OS_WIN
    QnDesktopResourceSearcher desktopSearcher(dynamic_cast<QGLWidget *>(mainWindow->viewport()));
    QnDesktopResourceSearcher::initStaticInstance(&desktopSearcher);
    QnDesktopResourceSearcher::instance().setLocal(true);
    QnResourceDiscoveryManager::instance()->addDeviceServer(&QnDesktopResourceSearcher::instance());
#endif

    QnResourceDiscoveryManager::instance()->setReady(true);
    QnResourceDiscoveryManager::instance()->start();

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

    QnResourceDiscoveryManager::instance()->stop();

    QnAppServerConnectionFactory::setEc2Connection(NULL);

    ec2ConnectionFactory.reset();

    QnAppServerConnectionFactory::setUrl(QUrl());

#ifdef Q_OS_WIN
    QnDesktopResourceSearcher::initStaticInstance(NULL);
#endif

    /* Write out settings. */
    qnSettings->setAudioVolume(QtvAudioDevice::instance()->volume());

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
