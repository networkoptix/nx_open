// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_context.h"

#include <QtWidgets/QApplication>

#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <core/resource/user_resource.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/analytics/analytics_entities_tree.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/intercom/intercom_manager.h>
#include <nx/vms/client/desktop/joystick/settings/manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/vms/client/desktop/system_health/system_health_state.h>
#include <nx/vms/client/desktop/system_health/system_internet_access_watcher.h>
#include <nx/vms/client/desktop/system_update/client_update_manager.h>
#include <nx/vms/client/desktop/system_update/server_update_tool.h>
#include <nx/vms/client/desktop/system_update/workbench_update_watcher.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/client/desktop/workbench/managers/settings_dialog_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/system_settings.h>
#include <statistics/statistics_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/instruments/gl_checker_instrument.h>
#include <ui/statistics/modules/actions_statistics_module.h>
#include <ui/statistics/modules/durations_statistics_module.h>
#include <ui/statistics/modules/graphics_statistics_module.h>
#include <ui/statistics/modules/users_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/watchers/workbench_desktop_camera_watcher.h>
#include <ui/workbench/watchers/workbench_layout_watcher.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>

#if defined(Q_OS_LINUX)
    #include <ui/workaround/x11_launcher_workaround.h>
#endif

using namespace nx::vms::client::desktop;

QnWorkbenchContext::QnWorkbenchContext(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    store(new SystemInternetAccessWatcher(systemContext));

    // FIXME: #sivanov Workaround interopration between those two.
    /* Layout watcher should be instantiated before snapshot manager because it can modify layout on adding. */
    m_layoutWatcher = instance<QnWorkbenchLayoutWatcher>();

    m_workbench.reset(new Workbench(this));

    m_userWatcher = systemContext->userWatcher();

    // Desktop camera must work in the normal mode only.
    if (qnRuntime->isDesktopMode())
        instance<QnWorkbenchDesktopCameraWatcher>();

    connect(m_userWatcher, &nx::vms::client::core::UserWatcher::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            emit userChanged(user);
            emit userIdChanged();
        });

    /* Create dependent objects. */
    m_menu.reset(new ui::action::Manager(this));
    m_joystickManager.reset(joystick::Manager::create(this));
    m_settingsDialogManager = std::make_unique<SettingsDialogManager>(this);
    m_display.reset(new QnWorkbenchDisplay(this));
    m_navigator.reset(new QnWorkbenchNavigator(this));
    m_intercomManager = std::make_unique<IntercomManager>(systemContext);

    // Adds statistics modules

    auto statisticsManager = statisticsModule()->manager();

    const auto actionsStatModule = instance<QnActionsStatisticsModule>();
    actionsStatModule->setActionManager(m_menu.data());
    statisticsManager->registerStatisticsModule(lit("actions"), actionsStatModule);

    const auto userStatModule = instance<QnUsersStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("users"), userStatModule);

    const auto graphicsStatModule = instance<QnGraphicsStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("graphics"), graphicsStatModule);

    const auto durationStatModule = instance<QnDurationStatisticsModule>();
    statisticsManager->registerStatisticsModule(lit("durations"), durationStatModule);

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionClosed,
        statisticsManager, &QnStatisticsManager::resetStatistics);

    store(new DefaultPasswordCamerasWatcher(systemContext));
    store(new AnalyticsEventsSearchTreeBuilder(systemContext));
    instance<SystemHealthState>();
    instance<QnGLCheckerInstrument>();
    instance<workbench::LocalNotificationsManager>();
    instance<Director>();
    store(new ServerUpdateTool(systemContext));
    instance<WorkbenchUpdateWatcher>();
    instance<ClientUpdateManager>();
    m_resourceTreeSettings.reset(new ResourceTreeSettings());

    initWorkarounds();
}

QnWorkbenchContext::~QnWorkbenchContext()
{
    /* Destroy typed subobjects in reverse order to how they were constructed. */
    QnInstanceStorage::clear();

    m_userWatcher = nullptr;
    m_layoutWatcher = nullptr;

    /* Destruction order of these objects is important. */
    m_resourceTreeSettings.reset();
    m_navigator.reset();
    m_display.reset();
    m_menu.reset();
    m_workbench.reset();
}

Workbench* QnWorkbenchContext::workbench() const
{
    return m_workbench.data();
}

ui::action::Manager* QnWorkbenchContext::menu() const
{
    return m_menu.data();
}

QnWorkbenchDisplay* QnWorkbenchContext::display() const
{
    return m_display.data();
}

QnWorkbenchNavigator* QnWorkbenchContext::navigator() const
{
    return m_navigator.data();
}

joystick::Manager* QnWorkbenchContext::joystickManager() const
{
    return m_joystickManager.get();
}

SettingsDialogManager* QnWorkbenchContext::settingsDialogManager() const
{
    return m_settingsDialogManager.get();
}

MainWindow* QnWorkbenchContext::mainWindow() const
{
    return m_mainWindow.data();
}

QWidget* QnWorkbenchContext::mainWindowWidget() const
{
    return mainWindow();
}

void QnWorkbenchContext::setMainWindow(MainWindow* mainWindow)
{
    m_mainWindow = mainWindow;
    systemContext()->restApiHelper()->setParent(mainWindow);
}

ResourceTreeSettings* QnWorkbenchContext::resourceTreeSettings() const
{
    return m_resourceTreeSettings.get();
}

nx::core::Watermark QnWorkbenchContext::watermark() const
{
    if (systemSettings()->watermarkSettings().useWatermark
        && user()
        && !accessController()->hasPowerUserPermissions()
        && !user()->getName().isEmpty())
    {
        return {systemSettings()->watermarkSettings(), user()->getName()};
    }
    return {};
}

QAction *QnWorkbenchContext::action(const ui::action::IDType id) const {
    return m_menu->action(id);
}

QnUserResourcePtr QnWorkbenchContext::user() const {
    if(m_userWatcher) {
        return m_userWatcher->user();
    } else {
        return QnUserResourcePtr();
    }
}

QString QnWorkbenchContext::userId() const
{
    const auto userPtr = user();
    return userPtr ? userPtr->getId().toString() : "";
}

bool QnWorkbenchContext::closingDown() const
{
    return m_closingDown;
}

void QnWorkbenchContext::setClosingDown(bool value)
{
    m_closingDown = value;
}

void QnWorkbenchContext::handleStartupParameters(const QnStartupParameters& startupParams)
{
    menu()->trigger(ui::action::ProcessStartupParametersAction,
        {Qn::StartupParametersRole, startupParams});
}

void QnWorkbenchContext::initWorkarounds()
{
    ui::action::IDType effectiveMaximizeActionId = ui::action::FullscreenAction;
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
        effectiveMaximizeActionId = ui::action::MaximizeAction;
#endif
    menu()->registerAlias(ui::action::EffectiveMaximizeAction, effectiveMaximizeActionId);
}

bool QnWorkbenchContext::isWorkbenchVisible() const
{
    return m_mainWindow && m_mainWindow->isWorkbenchVisible();
}
