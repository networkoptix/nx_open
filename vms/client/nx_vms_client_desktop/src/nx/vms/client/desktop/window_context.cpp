// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context.h"

#include <memory>

#include <QtCore/QPointer>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

#include <client/client_startup_parameters.h>
#include <core/resource/user_resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/menu/debug_actions_handler.h>
#include <nx/vms/client/desktop/joystick/joystick_settings_action_handler.h>
#include <nx/vms/client/desktop/layout/layout_action_handler.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/screen_recording/screen_recording_action_handler.h>
#include <nx/vms/client/desktop/showreel/showreel_actions_handler.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/connect_actions_handler.h>
#include <nx/vms/client/desktop/virtual_camera/virtual_camera_action_handler.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_handler.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <statistics/statistics_manager.h>
#include <ui/statistics/modules/actions_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/watchers/workbench_layout_aspect_ratio_watcher.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_state_manager.h>

#if defined(Q_OS_LINUX)
    #include <ui/workaround/x11_launcher_workaround.h>
#endif

namespace nx::vms::client::desktop {

QString WindowContext::kQmlPropertyName("windowContext");

using namespace workbench;

struct WindowContext::Private
{
    WindowContext* const q;

    std::unique_ptr<QnWorkbenchContext> workbenchContext;
    std::unique_ptr<Workbench> workbench;
    std::unique_ptr<menu::Manager> menuManager;
    std::unique_ptr<QnWorkbenchNavigator> navigator;

    std::unique_ptr<QnWorkbenchRenderWatcher> resourceWidgetRenderWatcher;
    std::unique_ptr<QnWorkbenchLayoutAspectRatioWatcher> layoutAspectRatioWatcher;
    std::unique_ptr<QnWorkbenchStreamSynchronizer> streamSynchronizer;
    std::unique_ptr<LocalNotificationsManager> localNotificationsManager;
    std::unique_ptr<WorkbenchStateManager> workbenchStateManager;

    // Action handlers.
    std::unique_ptr<ConnectActionsHandler> connectActionHandler;
    std::unique_ptr<DebugActionsHandler> debugActionHandler;
    std::unique_ptr<ShowreelActionsHandler> showreelActionsHandler;
    std::unique_ptr<JoystickSettingsActionHandler> joystickSettingsActionHandler;
    std::unique_ptr<ScreenRecordingActionHandler> screenRecordingActionHandler;
    std::unique_ptr<VirtualCameraActionHandler> virtualCameraActionHandler;
    std::unique_ptr<LayoutActionHandler> layoutActionHandler;
    std::unique_ptr<NotificationActionHandler> notificationActionHandler;

    void setupUnityLauncherWorkaround()
    {
        // In Ubuntu its launcher is configured to be shown when a non-fullscreen window has
        // appeared. In our case it means that launcher overlaps our fullscreen window when the
        // user opens any dialogs. To prevent such overlapping there was an attempt to hide unity
        // launcher when the main window has been activated. But now we can't hide launcher window
        // because there is no any visible window for it. Unity-3D launcher is like a 3D-effect
        // activated by compiz window manager. We can investigate possibilities of changing the
        // behavior of unity compiz plugin but now we just disable fullscreen for unity-3d desktop
        // session.
        menu::IDType effectiveMaximizeActionId = menu::FullscreenAction;
        #if defined Q_OS_LINUX
            if (QnX11LauncherWorkaround::isUnity3DSession())
                effectiveMaximizeActionId = menu::MaximizeAction;
        #endif
        menuManager->registerAlias(menu::EffectiveMaximizeAction, effectiveMaximizeActionId);
    }

    void initializeMenu()
    {
        menuManager = std::make_unique<menu::Manager>(q);
        setupUnityLauncherWorkaround();

        // Collect actions statistics.
        auto actionStatisticsModule = new QnActionsStatisticsModule(menuManager.get());
        actionStatisticsModule->setActionManager(menuManager.get());

        QString alias = "actions";
        if (int index = appContext()->sharedMemoryManager()->currentInstanceIndex(); index > 0)
            alias += QString::number(index);

        appContext()->statisticsModule()->manager()->registerStatisticsModule(
            alias,
            actionStatisticsModule);
    }
};

WindowContext::WindowContext(QObject* parent):
    QObject(parent),
    d(new Private{.q=this})
{
    // Create state manager before the context as it contains SessionAwareDelegates, trying to
    // register itself in the state manager.
    d->workbenchStateManager = std::make_unique<WorkbenchStateManager>(this);

    // TODO: #sivanov Do not require system context in the constructor.
    d->workbenchContext = std::make_unique<QnWorkbenchContext>(this,
        appContext()->currentSystemContext());
    d->workbench = std::make_unique<Workbench>(this);

    d->initializeMenu();

    // Depends on workbench and menu.
    d->navigator = std::make_unique<QnWorkbenchNavigator>(this);

    d->resourceWidgetRenderWatcher = std::make_unique<QnWorkbenchRenderWatcher>(this);

    // Depends on resourceWidgetRenderWatcher.
    d->layoutAspectRatioWatcher = std::make_unique<QnWorkbenchLayoutAspectRatioWatcher>(this);

    // Depends on resourceWidgetRenderWatcher.
    d->streamSynchronizer = std::make_unique<QnWorkbenchStreamSynchronizer>(this);
    d->localNotificationsManager = std::make_unique<LocalNotificationsManager>();
    d->workbenchContext->initialize();

    // Action handlers.
    d->connectActionHandler = std::make_unique<ConnectActionsHandler>(this);
    d->debugActionHandler = std::make_unique<DebugActionsHandler>(this);
    d->showreelActionsHandler = std::make_unique<ShowreelActionsHandler>(workbenchContext());
    d->joystickSettingsActionHandler = std::make_unique<JoystickSettingsActionHandler>(this);
    if (menu()->action(menu::ToggleScreenRecordingAction))
        d->screenRecordingActionHandler = std::make_unique<ScreenRecordingActionHandler>(this);
    d->virtualCameraActionHandler = std::make_unique<VirtualCameraActionHandler>(this);
    d->layoutActionHandler = std::make_unique<LayoutActionHandler>(this);
    d->notificationActionHandler = std::make_unique<NotificationActionHandler>(this);

    connect(appContext()->currentSystemContext(), &SystemContext::userChanged, this,
        &WindowContext::userIdChanged);

    connect(menu()->action(menu::InitialResourcesReceivedEvent), &QAction::triggered, this,
        [this]()
        {
            // Actually this signal should be sent when current system context is changed in the
            // window context, but for now we have only one system context for all systems, and it
            // is cleared and re-filled on each connection to another system.
            emit systemChanged();
        });
}

WindowContext::~WindowContext()
{
}

void WindowContext::registerQmlType()
{
    qmlRegisterUncreatableType<WindowContext>("nx.vms.client.desktop", 1, 0, "WindowContext",
        "Cannot create instance of WindowContext");
}

WindowContext* WindowContext::fromQmlContext(QObject* object)
{
    const auto qmlContext = QQmlEngine::contextForObject(object);
    return NX_ASSERT(qmlContext)
        ? qmlContext->contextProperty(kQmlPropertyName).value<WindowContext*>()
        : nullptr;
}

QWidget* WindowContext::mainWindowWidget() const
{
    return d->workbenchContext->mainWindowWidget();
}

SystemContext* WindowContext::system() const
{
    // Later on each Window Context will have it's own current system context and be able to switch
    // between them. For now we have only one current system context available.
    return appContext()->currentSystemContext();
}

void WindowContext::setCurrentSystem(SystemContext* value)
{
    NX_ASSERT(false, "NOT IMPLEMENTED YET");
}

QString WindowContext::userId() const
{
    if (auto user = system()->user())
        return user->getId().toString();
    return QString();
}

Workbench* WindowContext::workbench() const
{
    return d->workbench.get();
}

QnWorkbenchContext* WindowContext::workbenchContext() const
{
    return d->workbenchContext.get();
}

QnWorkbenchDisplay* WindowContext::display() const
{
    return workbenchContext()->display();
}

menu::Manager* WindowContext::menu() const
{
    return d->menuManager.get();
}

QnWorkbenchNavigator* WindowContext::navigator() const
{
    return d->navigator.get();
}

QnWorkbenchRenderWatcher* WindowContext::resourceWidgetRenderWatcher() const
{
    return d->resourceWidgetRenderWatcher.get();
}

QnWorkbenchStreamSynchronizer* WindowContext::streamSynchronizer() const
{
    return d->streamSynchronizer.get();
}

ShowreelActionsHandler* WindowContext::showreelActionsHandler() const
{
    return d->showreelActionsHandler.get();
}

NotificationActionHandler* WindowContext::notificationActionHandler() const
{
    return d->notificationActionHandler.get();
}

LocalNotificationsManager* WindowContext::localNotificationsManager() const
{
    return d->localNotificationsManager.get();
}

WorkbenchStateManager* WindowContext::workbenchStateManager() const
{
    return d->workbenchStateManager.get();
}

ConnectActionsHandler* WindowContext::connectActionsHandler() const
{
    return d->connectActionHandler.get();
}

void WindowContext::handleStartupParameters(const QnStartupParameters& startupParams)
{
    menu()->trigger(menu::ProcessStartupParametersAction,
        {Qn::StartupParametersRole, startupParams});
}

QQuickWindow* WindowContext::quickWindow() const
{
    // Eventually main window would itself be QQuickWindow, but for now just use the one which
    // always exists in both desktop and videowall modes.
    if (const auto mainWindow = qobject_cast<MainWindow*>(mainWindowWidget()))
        return mainWindow->quickWindow();

    return nullptr;
}

} // namespace nx::vms::client::desktop
