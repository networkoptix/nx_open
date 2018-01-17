#include "workbench_context.h"

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_startup_parameters.h>
#include <client/client_runtime_settings.h>
#include <client/client_message_processor.h>

#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_synchronizer.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>
#include <ui/workbench/watchers/workbench_layout_watcher.h>
#include <ui/workbench/watchers/workbench_desktop_camera_watcher.h>
#include <ui/workbench/workbench_welcome_screen.h>

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

#include <utils/common/app_info.h>

#include <watchers/cloud_status_watcher.h>

#include <nx/utils/log/log.h>
#include <client/client_module.h>

using namespace nx::client::desktop::ui;

QnWorkbenchContext::QnWorkbenchContext(QnWorkbenchAccessController* accessController, QObject* parent):
    QObject(parent),
    m_accessController(accessController),
    m_userWatcher(nullptr),
    m_layoutWatcher(nullptr),
    m_closingDown(false)
{
    /* Layout watcher should be instantiated before snapshot manager because it can modify layout on adding. */
    m_layoutWatcher = instance<QnWorkbenchLayoutWatcher>();
    m_snapshotManager.reset(new QnWorkbenchLayoutSnapshotManager(this));

    m_workbench.reset(new QnWorkbench(this));

    m_userWatcher = instance<QnWorkbenchUserWatcher>();

    instance<QnWorkbenchDesktopCameraWatcher>();

    connect(m_userWatcher, &QnWorkbenchUserWatcher::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            if (m_accessController)
                m_accessController->setUser(user);
            emit userChanged(user);
        });

    /* Create dependent objects. */
    m_synchronizer.reset(new QnWorkbenchSynchronizer(this));

    m_menu.reset(new nx::client::desktop::ui::action::Manager(this));
    m_display.reset(new QnWorkbenchDisplay(this));
    m_navigator.reset(new QnWorkbenchNavigator(this));

    // Adds statistics modules

    auto statisticsManager = commonModule()->instance<QnStatisticsManager>();

    const auto actionsStatModule = instance<QnActionsStatisticsModule>();
    actionsStatModule->setActionManager(m_menu.data());
    statisticsManager->registerStatisticsModule(lit("actions"), actionsStatModule);

    const auto userStatModule = instance<QnUsersStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("users"), userStatModule);

    const auto graphicsStatModule = instance<QnGraphicsStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("graphics"), graphicsStatModule);

    const auto durationStatModule = instance<QnDurationStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("durations"), durationStatModule);

    m_statisticsModule.reset(new QnControlsStatisticsModule());
    statisticsManager->registerStatisticsModule(lit("controls"), m_statisticsModule.data());

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened,
        statisticsManager, &QnStatisticsManager::resetStatistics);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived,
        statisticsManager, &QnStatisticsManager::sendStatistics);

    initWorkarounds();
}

QnWorkbenchContext::~QnWorkbenchContext() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    /* Destroy typed subobjects in reverse order to how they were constructed. */
    QnInstanceStorage::clear();

    m_userWatcher = nullptr;
    m_layoutWatcher = nullptr;
    m_accessController = nullptr;

    /* Destruction order of these objects is important. */
    m_statisticsModule.reset();
    m_navigator.reset();
    m_display.reset();
    m_menu.reset();
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

nx::client::desktop::ui::action::Manager* QnWorkbenchContext::menu() const
{
    return m_menu.data();
}

QnWorkbenchAccessController* QnWorkbenchContext::accessController() const
{
    return m_accessController;
}

void QnWorkbenchContext::setAccessController(QnWorkbenchAccessController* value)
{
    m_accessController = value;
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

MainWindow* QnWorkbenchContext::mainWindow() const
{
    return m_mainWindow.data();
}

void QnWorkbenchContext::setMainWindow(MainWindow* mainWindow)
{
    if (m_mainWindow == mainWindow)
        return;

    m_mainWindow = mainWindow;
    emit mainWindowChanged();
}


QAction *QnWorkbenchContext::action(const action::IDType id) const {
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

bool QnWorkbenchContext::connectUsingCustomUri(const nx::vms::utils::SystemUri& uri)
{
    if (!uri.isValid())
        return false;

    using namespace nx::vms::utils;

    SystemUri::Auth auth = uri.authenticator();
    QnEncodedCredentials credentials(auth.user, auth.password);

    switch (uri.clientCommand())
    {
        case SystemUri::ClientCommand::LoginToCloud:
        {
            NX_LOG(lit("Custom URI: Connecting to cloud"), cl_logDEBUG1);
            qnClientModule->cloudStatusWatcher()->setCredentials(credentials, true);
            break;
        }
        case SystemUri::ClientCommand::Client:
        {
            QString systemId = uri.systemId();
            if (systemId.isEmpty())
                return false;

            bool systemIsCloud = !QnUuid::fromStringSafe(systemId).isNull();

            auto systemUrl = nx::utils::Url::fromUserInput(systemId);
            NX_LOG(lit("Custom URI: Connecting to system %1").arg(systemUrl.toString()), cl_logDEBUG1);

            systemUrl.setUserName(auth.user);
            systemUrl.setPassword(auth.password);

            if (systemIsCloud)
            {
                qnClientModule->cloudStatusWatcher()->setCredentials(credentials, true);
                NX_LOG(lit("Custom URI: System is cloud, connecting to cloud first"), cl_logDEBUG1);
            }

            auto parameters = action::Parameters().withArgument(Qn::UrlRole, systemUrl);
            parameters.setArgument(Qn::ForceRole, true);
            parameters.setArgument(Qn::StoreSessionRole, false);
            menu()->trigger(action::ConnectAction, parameters);
            return true;

        }
        default:
            break;
    }
    return false;
}

bool QnWorkbenchContext::connectUsingCommandLineAuth(const QnStartupParameters& startupParams)
{
    /* Set authentication parameters from command line. */

    nx::utils::Url appServerUrl = startupParams.parseAuthenticationString();

    // TODO: #refactor System URI to support videowall
    if (!startupParams.videoWallGuid.isNull())
    {
        NX_ASSERT(appServerUrl.isValid());
        if (!appServerUrl.isValid())
        {
            return false;
        }

        appServerUrl.setUserName(startupParams.videoWallGuid.toString());
    }

    auto params = action::Parameters().withArgument(Qn::UrlRole, appServerUrl);
    params.setArgument(Qn::ForceRole, true);
    params.setArgument(Qn::StoreSessionRole, false);
    menu()->trigger(action::ConnectAction, params);
    return true;
}

bool QnWorkbenchContext::handleStartupParameters(const QnStartupParameters& startupParams)
{
    /* Process input files. */
    bool haveInputFiles = false;
    {
        auto window = qobject_cast<nx::client::desktop::ui::MainWindow*>(mainWindow());
        NX_ASSERT(window);

        bool skipArg = true;
        for (const auto& arg : qApp->arguments())
        {
            if (!skipArg)
                haveInputFiles |= window && window->handleMessage(arg);
            skipArg = false;
        }
    }

    /* If no input files were supplied --- open welcome page.
    * Do not try to connect in the following cases:
    * * we were not connected and clicked "Open in new window"
    * * we have opened exported exe-file
    * Otherwise we should try to connect or show welcome page.
    */
    if (const auto welcomeScreen = mainWindow()->welcomeScreen())
        welcomeScreen->setVisibleControls(true);

    if (!connectUsingCustomUri(startupParams.customUri)
        && startupParams.instantDrop.isEmpty()
        && !haveInputFiles
        && !connectUsingCommandLineAuth(startupParams)
        )
    {
        return false;
    }

    if (!startupParams.videoWallGuid.isNull())
    {
        menu()->trigger(action::DelayedOpenVideoWallItemAction, action::Parameters()
                                 .withArgument(Qn::VideoWallGuidRole, startupParams.videoWallGuid)
                                 .withArgument(Qn::VideoWallItemGuidRole, startupParams.videoWallItemGuid));
    }
    else if (!startupParams.delayedDrop.isEmpty())
    { /* Drop resources if needed. */
        NX_ASSERT(startupParams.instantDrop.isEmpty());

        QByteArray data = QByteArray::fromBase64(startupParams.delayedDrop.toLatin1());
        menu()->trigger(action::DelayedDropResourcesAction, {Qn::SerializedDataRole, data});
    }
    else if (!startupParams.instantDrop.isEmpty())
    {
        QByteArray data = QByteArray::fromBase64(startupParams.instantDrop.toLatin1());
        menu()->trigger(action::InstantDropResourcesAction, {Qn::SerializedDataRole, data});
    }

    /* Show beta version warning message for the main instance only */
    bool showBetaWarning = QnAppInfo::beta()
        && !startupParams.allowMultipleClientInstances
        && qnRuntime->isDesktopMode()
        && !qnRuntime->isDevMode()
        && startupParams.customUri.isNull();

    if (showBetaWarning)
        action(action::BetaVersionMessageAction)->trigger();

#ifdef _DEBUG
    /* Show FPS in debug. */
    menu()->trigger(action::ShowFpsAction);
#endif

    return true;
}

void QnWorkbenchContext::initWorkarounds()
{
    instance<QnFglrxFullScreen>(); /* Init fglrx workaround. */

    action::IDType effectiveMaximizeActionId = action::FullscreenAction;
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
        effectiveMaximizeActionId = action::MaximizeAction;
#endif
    menu()->registerAlias(action::EffectiveMaximizeAction, effectiveMaximizeActionId);
}

