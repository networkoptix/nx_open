#include "workbench_context.h"

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_startup_parameters.h>
#include <client/client_runtime_settings.h>
#include <client/client_message_processor.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_synchronizer.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_license_notifier.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>
#include <ui/workbench/watchers/workbench_user_email_watcher.h>
#include <ui/workbench/watchers/workbench_layout_watcher.h>
#include <ui/workbench/watchers/workbench_desktop_camera_watcher.h>

#include <statistics/statistics_manager.h>
#include <ui/statistics/modules/actions_statistics_module.h>
#include <ui/statistics/modules/users_statistics_module.h>
#include <ui/statistics/modules/graphics_statistics_module.h>
#include <ui/statistics/modules/durations_statistics_module.h>
#include <ui/statistics/modules/controls_statistics_module.h>

#include <ui/widgets/main_window.h>
#include <ui/workaround/fglrx_full_screen.h>
#ifdef Q_OS_LINUX
#include <ui/workaround/x11_launcher_workaround.h>
#endif

#include <nx/vms/utils/system_uri.h>

#include <watchers/cloud_status_watcher.h>

QnWorkbenchContext::QnWorkbenchContext(QObject *parent):
    QObject(parent),
    m_userWatcher(NULL),
    m_layoutWatcher(NULL),
    m_closingDown(false)
{
    /* Layout watcher should be instantiated before snapshot manager because it can modify layout on adding. */
    m_layoutWatcher = instance<QnWorkbenchLayoutWatcher>();
    m_snapshotManager.reset(new QnWorkbenchLayoutSnapshotManager(this));

    /*
     * Access controller should be initialized early because a lot of other modules use it.
     * It depends on snapshotManager only,
     */
    m_accessController.reset(new QnWorkbenchAccessController(this));

    m_workbench.reset(new QnWorkbench(this));

    m_userWatcher = instance<QnWorkbenchUserWatcher>();

    instance<QnWorkbenchDesktopCameraWatcher>();

    connect(m_userWatcher,  SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SIGNAL(userChanged(const QnUserResourcePtr &)));

    /* Create dependent objects. */
    m_synchronizer.reset(new QnWorkbenchSynchronizer(this));

    m_menu.reset(new QnActionManager(this));
    m_display.reset(new QnWorkbenchDisplay(this));
    m_navigator.reset(new QnWorkbenchNavigator(this));

	if (!qnRuntime->isActiveXMode())
		instance<QnWorkbenchLicenseNotifier>(); // TODO: #Elric belongs here?

    // Adds statistics modules

    const auto actionsStatModule = instance<QnActionsStatisticsModule>();
    actionsStatModule->setActionManager(m_menu.data()); // TODO: #ynikitenkov refactor QnActionManager to singleton
    qnStatisticsManager->registerStatisticsModule(lit("actions"), actionsStatModule);

    const auto userStatModule = instance<QnUsersStatisticsModule>();
    qnStatisticsManager->registerStatisticsModule(lit("users"), userStatModule);

    const auto graphicsStatModule = instance<QnGraphicsStatisticsModule>();
    qnStatisticsManager->registerStatisticsModule(lit("graphics"), graphicsStatModule);

    const auto durationStatModule = instance<QnDurationStatisticsModule>();
    qnStatisticsManager->registerStatisticsModule(lit("durations"), durationStatModule);

    m_statisticsModule.reset(new QnControlsStatisticsModule());
    qnStatisticsManager->registerStatisticsModule(lit("controls"), m_statisticsModule.data());

    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::connectionOpened
                     , qnStatisticsManager, &QnStatisticsManager::resetStatistics);
    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::initialResourcesReceived
                     , qnStatisticsManager, &QnStatisticsManager::sendStatistics);

    initWorkarounds();
}

QnWorkbenchContext::~QnWorkbenchContext() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    /* Destroy typed subobjects in reverse order to how they were constructed. */
    QnInstanceStorage::clear();
    m_userWatcher = NULL;
    m_layoutWatcher = NULL;

    /* Destruction order of these objects is important. */
    m_statisticsModule.reset();
    m_navigator.reset();
    m_display.reset();
    m_menu.reset();
    m_accessController.reset();
    m_snapshotManager.reset();
    m_synchronizer.reset();
    m_workbench.reset();
}

QnWorkbench* QnWorkbenchContext::workbench() const
{
    return m_workbench.data();
}

QnWorkbenchLayoutSnapshotManager* QnWorkbenchContext::snapshotManager() const
{
    return m_snapshotManager.data();
}

QnWorkbenchAccessController* QnWorkbenchContext::accessController() const
{
    return m_accessController.data();
}

QnWorkbenchDisplay* QnWorkbenchContext::display() const
{
    return m_display.data();
}

QnWorkbenchNavigator* QnWorkbenchContext::navigator() const
{
    return m_navigator.data();
}

QnControlsStatisticsModule* QnWorkbenchContext::statisticsModule() const
{
    return m_statisticsModule.data();
}

QWidget* QnWorkbenchContext::mainWindow() const
{
    return m_mainWindow.data();
}

void QnWorkbenchContext::setMainWindow(QWidget *mainWindow)
{
    if (m_mainWindow == mainWindow)
        return;

    m_mainWindow = mainWindow;
    emit mainWindowChanged();
}


QAction *QnWorkbenchContext::action(const QnActions::IDType id) const {
    return m_menu->action(id);
}

QnUserResourcePtr QnWorkbenchContext::user() const {
    if(m_userWatcher) {
        return m_userWatcher->user();
    } else {
        return QnUserResourcePtr();
    }
}

void QnWorkbenchContext::setUserName(const QString &userName) {
    if(m_userWatcher)
        m_userWatcher->setUserName(userName);
}


bool QnWorkbenchContext::closingDown() const
{
    return m_closingDown;
}

void QnWorkbenchContext::setClosingDown(bool value)
{
    m_closingDown = value;
}

bool QnWorkbenchContext::handleStartupParameters(const QnStartupParameters& startupParams)
{
    /* Process input files. */
    bool haveInputFiles = false;
    {
        QnMainWindow* window = qobject_cast<QnMainWindow*>(mainWindow());
        NX_ASSERT(window);

        bool skipArg = true;
        for (const auto& arg : qApp->arguments())
        {
            if (!skipArg)
                haveInputFiles |= window && window->handleMessage(arg);
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

                menu()->trigger(QnActions::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, systemUrl));
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
                return false;
            }
            appServerUrl.setUserName(startupParams.videoWallGuid.toString());
        }
        menu()->trigger(QnActions::ConnectAction, QnActionParameters().withArgument(Qn::UrlRole, appServerUrl));
    }

    if (!startupParams.videoWallGuid.isNull())
    {
        menu()->trigger(QnActions::DelayedOpenVideoWallItemAction, QnActionParameters()
                                 .withArgument(Qn::VideoWallGuidRole, startupParams.videoWallGuid)
                                 .withArgument(Qn::VideoWallItemGuidRole, startupParams.videoWallItemGuid));
    }
    else if (!startupParams.delayedDrop.isEmpty())
    { /* Drop resources if needed. */
        NX_ASSERT(startupParams.instantDrop.isEmpty());

        QByteArray data = QByteArray::fromBase64(startupParams.delayedDrop.toLatin1());
        menu()->trigger(QnActions::DelayedDropResourcesAction, QnActionParameters().withArgument(Qn::SerializedDataRole, data));
    }
    else if (!startupParams.instantDrop.isEmpty())
    {
        QByteArray data = QByteArray::fromBase64(startupParams.instantDrop.toLatin1());
        menu()->trigger(QnActions::InstantDropResourcesAction, QnActionParameters().withArgument(Qn::SerializedDataRole, data));
    }

    /* Show beta version warning message for the main instance only */
    bool showBetaWarning = QnAppInfo::beta()
        && !startupParams.allowMultipleClientInstances
        && qnRuntime->isDesktopMode()
        && !qnRuntime->isDevMode()
        && startupParams.customUri.isNull();

    if (showBetaWarning)
        action(QnActions::BetaVersionMessageAction)->trigger();

#ifdef _DEBUG
    /* Show FPS in debug. */
    menu()->trigger(QnActions::ShowFpsAction);
#endif

    return true;
}

void QnWorkbenchContext::initWorkarounds()
{
    instance<QnFglrxFullScreen>(); /* Init fglrx workaround. */

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
    menu()->registerAlias(QnActions::EffectiveMaximizeAction, effectiveMaximizeActionId);
}

