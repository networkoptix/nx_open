#include "actions.h"

#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_criterion.h>

#include <client/client_runtime_settings.h>

#include <nx/client/desktop/ui/actions/menu_factory.h>
#include <nx/client/desktop/ui/actions/action_conditions.h>
#include <nx/client/desktop/ui/actions/action_factories.h>
#include <nx/client/desktop/ui/actions/action_text_factories.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/fusion/model_functions.h>

#include <nx/network/app_info.h>

#include <nx/utils/app_info.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnActions, IDType)

using namespace nx::client::desktop::ui::action;

void QnActions::initialize(Manager* manager, Action* root)
{

    MenuFactory factory(manager, root);

    using namespace QnResourceCriterionExpressions;

    /* Actions that are not assigned to any menu. */

    factory(QnActions::ShowFpsAction).
        flags(GlobalHotkey).
        text(lit("Show FPS")).
        shortcut(lit("Ctrl+Alt+F")).
        autoRepeat(false);

    factory(QnActions::ShowDebugOverlayAction).
        flags(GlobalHotkey).
        text(lit("Show Debug")).
        shortcut(lit("Ctrl+Alt+D")).
        autoRepeat(false);

    factory(QnActions::DropResourcesAction).
        flags(ResourceTarget | WidgetTarget | LayoutItemTarget | LayoutTarget | SingleTarget | MultiTarget).
        mode(DesktopMode).
        text(tr("Drop Resources"));

    factory(QnActions::DropResourcesIntoNewLayoutAction).
        flags(ResourceTarget | WidgetTarget | LayoutItemTarget | LayoutTarget | SingleTarget | MultiTarget).
        mode(DesktopMode).
        text(tr("Drop Resources into New Layout"));

    factory(QnActions::DelayedOpenVideoWallItemAction).
        flags(NoTarget).
        text(tr("Delayed Open Video Wall"));

    factory(QnActions::DelayedDropResourcesAction).
        flags(NoTarget).
        mode(DesktopMode).
        text(tr("Delayed Drop Resources"));

    factory(QnActions::InstantDropResourcesAction).
        flags(NoTarget).
        mode(DesktopMode).
        text(tr("Instant Drop Resources"));

    factory(QnActions::MoveCameraAction).
        flags(ResourceTarget | SingleTarget | MultiTarget).
        requiredTargetPermissions(Qn::RemovePermission).
        text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->resourcePool(),
            tr("Move Devices"),
            tr("Move Cameras")
        )).
        condition(hasFlags(Qn::network));

    factory(QnActions::NextLayoutAction).
        flags(GlobalHotkey).
        mode(DesktopMode).
        text(tr("Next Layout")).
        shortcut(lit("Ctrl+Tab")).
        autoRepeat(false);

    factory(QnActions::PreviousLayoutAction).
        flags(GlobalHotkey).
        mode(DesktopMode).
        text(tr("Previous Layout")).
        shortcut(lit("Ctrl+Shift+Tab")).
        autoRepeat(false);

    factory(QnActions::SelectAllAction).
        flags(GlobalHotkey).
        text(tr("Select All")).
        shortcut(lit("Ctrl+A")).
        shortcutContext(Qt::WidgetWithChildrenShortcut).
        autoRepeat(false);

    factory(QnActions::SelectionChangeAction).
        flags(NoTarget).
        text(tr("Selection Changed"));

    factory(QnActions::PreferencesLicensesTabAction).
        flags(NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::PreferencesSmtpTabAction).
        flags(NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::PreferencesNotificationTabAction).
        flags(NoTarget).
        icon(qnSkin->icon("events/filter.png")).
        text(tr("Filter..."));

    factory(QnActions::PreferencesCloudTabAction).
        flags(NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::ConnectAction).
        flags(NoTarget);

    factory(QnActions::ConnectToCloudSystemAction).
        flags(Tree | NoTarget).
        text(tr("Connect to System")).
        condition(new TreeNodeTypeCondition(Qn::CloudSystemNode, manager));

    factory(QnActions::ReconnectAction).
        flags(NoTarget);

    factory(QnActions::FreespaceAction).
        flags(GlobalHotkey).
        mode(DesktopMode).
        text(tr("Go to Freespace Mode")).
        shortcut(lit("F11")).
        autoRepeat(false);

    factory(QnActions::WhatsThisAction).
        flags(NoTarget).
        text(tr("Help")).
        icon(qnSkin->icon("titlebar/window_question.png"));

    factory(QnActions::CameraDiagnosticsAction).
        mode(DesktopMode).
        flags(ResourceTarget | SingleTarget).
        dynamicText(new DevicesNameTextFactory(
            QnCameraDeviceStringSet(
                tr("Device Diagnostics..."),
                tr("Camera Diagnostics..."),
                tr("I/O Module Diagnostics...")
            ), manager)).
        condition(condition::hasFlags(Qn::live_cam, Any, manager));

    factory(QnActions::OpenBusinessLogAction).
        flags(NoTarget | SingleTarget | MultiTarget | ResourceTarget
            | LayoutItemTarget | WidgetTarget | GlobalHotkey).
        mode(DesktopMode).
        requiredGlobalPermission(Qn::GlobalViewLogsPermission).
        icon(qnSkin->icon("events/log.png")).
        shortcut(lit("Ctrl+L")).
        text(tr("Event Log..."));

    factory(QnActions::OpenBusinessRulesAction).
        mode(DesktopMode).
        flags(NoTarget | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Event Rules..."));

    factory(QnActions::OpenFailoverPriorityAction).
        mode(DesktopMode).
        flags(NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Failover Priority..."));

    factory(QnActions::OpenBackupCamerasAction).
        mode(DesktopMode).
        flags(NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Cameras to Backup..."));

    factory(QnActions::StartVideoWallControlAction).
        flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Control Video Wall")).
        condition(
            ConditionPtr(new StartVideoWallControlCondition(manager))
            && !condition::isSafeMode(manager)
        );

    factory(QnActions::PushMyScreenToVideowallAction).
        flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Push my screen")).
        condition(
            ConditionPtr(new DesktopCameraCondition(manager))
            && !condition::isSafeMode(manager)
        );

    factory(QnActions::QueueAppRestartAction).
        flags(NoTarget).
        text(tr("Restart application"));

    factory(QnActions::SelectTimeServerAction).
        flags(NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Select Time Server"));

    factory(QnActions::PtzActivatePresetAction).
        flags(SingleTarget | WidgetTarget).
        text(tr("Go To Saved Position")).
        requiredTargetPermissions(Qn::WritePtzPermission).
        condition(new PtzCondition(Qn::PresetsPtzCapability, false, manager));

    factory(QnActions::PtzActivateTourAction).
        flags(SingleTarget | WidgetTarget).
        text(tr("Activate PTZ Tour")).
        requiredTargetPermissions(Qn::WritePtzPermission).
        condition(new PtzCondition(Qn::ToursPtzCapability, false, manager));

    factory(QnActions::PtzActivateObjectAction).
        flags(SingleTarget | WidgetTarget).
        text(tr("Activate PTZ Object")).
        requiredTargetPermissions(Qn::WritePtzPermission);

    /* Context menu actions. */

    factory(QnActions::FitInViewAction).
        flags(Scene | NoTarget).
        text(tr("Fit in View"));

    factory().
        flags(Scene).
        separator();

    factory(QnActions::MainMenuAction).
        flags(GlobalHotkey).
        mode(DesktopMode).
        text(tr("Main Menu")).
        shortcut(lit("Alt+Space"), Builder::Mac, true).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/main_menu.png"));

    factory(QnActions::OpenLoginDialogAction).
        flags(Main | GlobalHotkey).
        mode(DesktopMode).
        text(tr("Connect to Server...")).
        shortcut(lit("Ctrl+Shift+C")).
        autoRepeat(false);

    factory(QnActions::DisconnectAction).
        flags(Main | GlobalHotkey).
        mode(DesktopMode).
        text(tr("Disconnect from Server")).
        autoRepeat(false).
        shortcut(lit("Ctrl+Shift+D")).
        condition(new LoggedInCondition(manager));

    factory(QnActions::ResourcesModeAction).
        flags(Main).
        mode(DesktopMode).
        text(tr("Browse Local Files")).
        toggledText(tr("Show Welcome Screen")).
        condition(new BrowseLocalFilesCondition(manager));

    factory().
        flags(Main).
        separator();

    factory(QnActions::TogglePanicModeAction).
        flags(GlobalHotkey | DevMode).
        mode(DesktopMode).
        text(tr("Start Panic Recording")).
        toggledText(tr("Stop Panic Recording")).
        autoRepeat(false).
        shortcut(lit("Ctrl+P")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new PanicCondition(manager));

    factory().
        flags(Main | Tree).
        separator();

    factory().
        flags(Main | TitleBar | Tree | SingleTarget | ResourceTarget).
        text(tr("New..."));

    factory.beginSubMenu();
    {
        factory(QnActions::OpenNewTabAction).
            flags(Main | TitleBar | SingleTarget | NoTarget | GlobalHotkey).
            mode(DesktopMode).
            text(tr("Tab")).
            pulledText(tr("New Tab")).
            shortcut(lit("Ctrl+T")).
            autoRepeat(false). /* Technically, it should be auto-repeatable, but we don't want the user opening 100500 layouts and crashing the client =). */
            icon(qnSkin->icon("titlebar/new_layout.png"));

        factory(QnActions::OpenNewWindowAction).
            flags(Main | GlobalHotkey).
            mode(DesktopMode).
            text(tr("Window")).
            pulledText(tr("New Window")).
            shortcut(lit("Ctrl+N")).
            autoRepeat(false).
            condition(new LightModeCondition(Qn::LightModeNoNewWindow, manager));

        factory().
            flags(Main).
            separator();

        factory(QnActions::NewUserAction).
            flags(Main | Tree).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("User...")).
            pulledText(tr("New User...")).
            condition(
                ConditionPtr(new TreeNodeTypeCondition(Qn::UsersNode, manager))
                && !condition::isSafeMode(manager)
            ).
            autoRepeat(false);

        factory(QnActions::NewVideoWallAction).
            flags(Main).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("Video Wall...")).
            pulledText(tr("New Video Wall...")).
            condition(!condition::isSafeMode(manager)).
            autoRepeat(false);

        factory(QnActions::NewWebPageAction).
            flags(Main | Tree).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("Web Page...")).
            pulledText(tr("New Web Page...")).
            condition(
                ConditionPtr(new TreeNodeTypeCondition(Qn::WebPagesNode, manager))
                && !condition::isSafeMode(manager)
            ).
            autoRepeat(false);

        factory(QnActions::NewLayoutTourAction).
            flags(Main | Tree | NoTarget).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("Layout Tour...")).
            pulledText(tr("New Layout Tour...")).
            condition(
            (
                ConditionPtr(new TreeNodeTypeCondition(Qn::LayoutsNode, manager))
                || ConditionPtr(new TreeNodeTypeCondition(Qn::LayoutToursNode, manager))
                )
                && !condition::isSafeMode(manager)
            ).
            autoRepeat(false);

    }
    factory.endSubMenu();

    factory(QnActions::NewUserLayoutAction).
        flags(Tree | SingleTarget | ResourceTarget | NoTarget).
        text(tr("New Layout...")).
        condition(
            ConditionPtr(new NewUserLayoutCondition(manager))
            && !condition::isSafeMode(manager)
        );

    factory(QnActions::OpenCurrentUserLayoutMenu).
        flags(TitleBar | SingleTarget | NoTarget).
        text(tr("Open Layout...")).
        childFactory(new OpenCurrentUserLayoutFactory(manager)).
        icon(qnSkin->icon("titlebar/dropdown.png"));

    factory().
        flags(TitleBar).
        separator();

    factory().
        flags(Main | Tree | Scene).
        mode(DesktopMode).
        text(tr("Open..."));

    factory.beginSubMenu();
    {
        factory(QnActions::OpenFileAction).
            flags(Main | Scene | NoTarget | GlobalHotkey).
            mode(DesktopMode).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
            text(tr("File(s)...")).
            shortcut(lit("Ctrl+O")).
            autoRepeat(false);

        factory(QnActions::OpenFolderAction).
            flags(Main | Scene).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
            text(tr("Folder..."));

        factory().separator().
            flags(Main);

        factory(QnActions::WebClientAction).
            flags(Main | Tree | NoTarget).
            text(tr("Web Client...")).
            pulledText(tr("Open Web Client...")).
            autoRepeat(false).
            condition(
                ConditionPtr(new LoggedInCondition(manager))
                && ConditionPtr(new TreeNodeTypeCondition({Qn::CurrentSystemNode, Qn::ServersNode}, manager))
            );

    } factory.endSubMenu();

    factory(QnActions::SaveCurrentLayoutAction).
        mode(DesktopMode).
        flags(Scene | NoTarget | GlobalHotkey | IntentionallyAmbiguous).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::SavePermission).
        text(tr("Save Current Layout")).
        shortcut(lit("Ctrl+S")).
        autoRepeat(false). /* There is no point in saving the same layout many times in a row. */
        condition(new SaveLayoutCondition(true, manager));

    factory(QnActions::SaveCurrentLayoutAsAction).
        mode(DesktopMode).
        flags(Scene | NoTarget | GlobalHotkey).
        text(tr("Save Current Layout As...")).
        shortcut(lit("Ctrl+Shift+S")).
        shortcut(lit("Ctrl+Alt+S"), Builder::Windows, true).
        autoRepeat(false).
        condition(
            ConditionPtr(new LoggedInCondition(manager))
            && ConditionPtr(new SaveLayoutAsCondition(true, manager))
        );

    factory(QnActions::ShareLayoutAction).
        mode(DesktopMode).
        flags(SingleTarget | ResourceTarget).
        text(lit("Share_Layout_with")).
        autoRepeat(false).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(!condition::isSafeMode(manager));

    factory(QnActions::SaveCurrentVideoWallReviewAction).
        flags(Main | Scene | NoTarget | GlobalHotkey | IntentionallyAmbiguous).
        mode(DesktopMode).
        text(tr("Save Video Wall View")).
        shortcut(lit("Ctrl+S")).
        autoRepeat(false).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        condition(
            !condition::isSafeMode(manager)
            && ConditionPtr(new SaveVideowallReviewCondition(true, manager))
        );

    factory(QnActions::DropOnVideoWallItemAction).
        flags(ResourceTarget | LayoutItemTarget | LayoutTarget | VideoWallItemTarget | SingleTarget | MultiTarget).
        text(tr("Drop Resources")).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        condition(!condition::isSafeMode(manager));

    factory().
        flags(Main).
        separator();

    const bool screenRecordingSupported = nx::utils::AppInfo::isWindows();
    if (screenRecordingSupported && qnRuntime->isDesktopMode())
    {
        factory(QnActions::ToggleScreenRecordingAction).
            flags(Main | GlobalHotkey).
            mode(DesktopMode).
            text(tr("Start Screen Recording")).
            toggledText(tr("Stop Screen Recording")).
            shortcut(lit("Alt+R")).
            shortcut(Qt::Key_MediaRecord).
            shortcutContext(Qt::ApplicationShortcut).
            autoRepeat(false);

        factory().
            flags(Main).
            separator();
    }

    factory(QnActions::EscapeHotkeyAction).
        flags(GlobalHotkey).
        mode(DesktopMode).
        autoRepeat(false).
        shortcut(lit("Esc")).
        text(tr("Stop current action"));

    factory(QnActions::FullscreenAction).
        flags(NoTarget).
        mode(DesktopMode).
        text(tr("Go to Fullscreen")).
        toggledText(tr("Exit Fullscreen")).
        icon(qnSkin->icon("titlebar/window_maximize.png", "titlebar/window_restore.png"));


    factory(QnActions::MinimizeAction).
        flags(NoTarget).
        text(tr("Minimize")).
        icon(qnSkin->icon("titlebar/window_minimize.png"));

    factory(QnActions::MaximizeAction).
        flags(NoTarget).
        text(tr("Maximize")).
        toggledText(tr("Restore Down")).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/window_maximize.png", "titlebar/window_restore.png"));


    factory(QnActions::FullscreenMaximizeHotkeyAction).
        flags(GlobalHotkey).
        mode(DesktopMode).
        autoRepeat(false).
        shortcut(lit("Alt+Enter")).
        shortcut(lit("Alt+Return")).
        shortcut(lit("Ctrl+F"), Builder::Mac, true).
        shortcutContext(Qt::ApplicationShortcut);

    factory(QnActions::VersionMismatchMessageAction).
        flags(NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(lit("Show Version Mismatch Message"));

    factory(QnActions::BetaVersionMessageAction).
        flags(NoTarget).
        mode(DesktopMode).
        text(lit("Show Beta Version Warning Message"));

    factory(QnActions::AllowStatisticsReportMessageAction).
        flags(NoTarget).
        mode(DesktopMode).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(lit("Ask About Statistics Reporting"));

    factory(QnActions::BrowseUrlAction).
        flags(NoTarget).
        mode(DesktopMode).
        text(tr("Open in Browser..."));

    factory(QnActions::SystemAdministrationAction).
        flags(Main | Tree | GlobalHotkey).
        mode(DesktopMode).
        text(tr("System Administration...")).
        shortcut(lit("Ctrl+Alt+A")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new TreeNodeTypeCondition({Qn::CurrentSystemNode, Qn::ServersNode}, manager));

    factory(QnActions::SystemUpdateAction).
        flags(NoTarget).
        text(tr("System Update...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::UserManagementAction).
        flags(Main | Tree).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("User Management...")).
        condition(new TreeNodeTypeCondition(Qn::UsersNode, manager));

    factory(QnActions::PreferencesGeneralTabAction).
        flags(Main).
        text(tr("Local Settings...")).
        //shortcut(lit("Ctrl+P")).
        role(QAction::PreferencesRole).
        autoRepeat(false);

    factory(QnActions::OpenAuditLogAction).
        flags(Main).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Audit Trail..."));

    factory(QnActions::OpenBookmarksSearchAction).
        flags(Main | GlobalHotkey).
        requiredGlobalPermission(Qn::GlobalViewBookmarksPermission).
        text(tr("Bookmark Search...")).
        shortcut(lit("Ctrl+B")).
        autoRepeat(false);

    factory(QnActions::LoginToCloud).
        flags(NoTarget).
        text(tr("Log in to %1...", "Log in to Nx Cloud").arg(nx::network::AppInfo::cloudName()));

    factory(QnActions::LogoutFromCloud).
        flags(NoTarget).
        text(tr("Log out from %1", "Log out from Nx Cloud").arg(nx::network::AppInfo::cloudName()));

    factory(QnActions::OpenCloudMainUrl).
        flags(NoTarget).
        text(tr("Open %1 Portal...", "Open Nx Cloud Portal").arg(nx::network::AppInfo::cloudName()));

    factory(QnActions::OpenCloudManagementUrl).
        flags(NoTarget).
        text(tr("Account Settings..."));

    factory(QnActions::HideCloudPromoAction).
        flags(NoTarget);

    factory(QnActions::OpenCloudRegisterUrl).
        flags(NoTarget).
        text(tr("Create Account..."));

    factory().
        flags(Main).
        separator();

    factory(QnActions::BusinessEventsAction).
        flags(GlobalHotkey).
        mode(DesktopMode).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Event Rules...")).
        icon(qnSkin->icon("events/settings.png")).
        shortcut(lit("Ctrl+E")).
        autoRepeat(false);

    factory(QnActions::CameraListAction).
        flags(GlobalHotkey).
        mode(DesktopMode).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->resourcePool(),
            tr("Devices List"),
            tr("Cameras List")
        )).
        shortcut(lit("Ctrl+M")).
        autoRepeat(false);

    factory(QnActions::MergeSystems).
        flags(Main | Tree).
        text(tr("Merge Systems...")).
        condition(
            ConditionPtr(new TreeNodeTypeCondition({Qn::CurrentSystemNode, Qn::ServersNode}, manager))
            && !condition::isSafeMode(manager)
            && ConditionPtr(new RequiresOwnerCondition(manager))
        );

    factory().
        flags(Main).
        separator();

    factory(QnActions::AboutAction).
        flags(Main | GlobalHotkey).
        mode(DesktopMode).
        text(tr("About...")).
        shortcut(lit("F1")).
        shortcutContext(Qt::ApplicationShortcut).
        role(QAction::AboutRole).
        autoRepeat(false);

    factory().
        flags(Main).
        separator();

    factory(QnActions::ExitAction).
        flags(Main | GlobalHotkey).
        text(tr("Exit")).
        shortcut(lit("Alt+F4")).
        shortcut(lit("Ctrl+Q"), Builder::Mac, true).
        shortcutContext(Qt::ApplicationShortcut).
        role(QAction::QuitRole).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/window_close.png")).
        iconVisibleInMenu(false);

    factory(QnActions::DelayedForcedExitAction).
        flags(NoTarget);

    factory(QnActions::BeforeExitAction).
        flags(NoTarget);

    factory().
        flags(Tree | SingleTarget | ResourceTarget).
        childFactory(new EdgeNodeFactory(manager)).
        text(tr("Server...")).
        condition(new TreeNodeTypeCondition(Qn::EdgeNode, manager));

    factory().
        flags(Tree | SingleTarget | ResourceTarget).
        separator().
        condition(new TreeNodeTypeCondition(Qn::EdgeNode, manager));

    /* Resource actions. */
    factory(QnActions::OpenInLayoutAction)
        .flags(SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .requiredTargetPermissions(Qn::LayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
        .text(tr("Open in Layout"))
        .condition(new OpenInLayoutCondition(manager));

    factory(QnActions::OpenInCurrentLayoutAction).
        flags(Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
        text(tr("Open")).
        conditionalText(tr("Monitor"), hasFlags(Qn::server), All).
        condition(new OpenInCurrentLayoutCondition(manager));

    factory(QnActions::OpenInNewLayoutAction).
        mode(DesktopMode).
        flags(Tree | Scene | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget).
        text(tr("Open in New Tab")).
        conditionalText(tr("Monitor in New Tab"), hasFlags(Qn::server), All).
        condition(new OpenInNewEntityCondition(manager));

    factory(QnActions::OpenInAlarmLayoutAction).
        mode(DesktopMode).
        flags(SingleTarget | MultiTarget | ResourceTarget).
        text(tr("Open in Alarm Layout"));

    factory(QnActions::OpenInNewWindowAction).
        mode(DesktopMode).
        flags(Tree | Scene | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget).
        text(tr("Open in New Window")).
        conditionalText(tr("Monitor in New Window"), hasFlags(Qn::server), All).
        condition(
            ConditionPtr(new OpenInNewEntityCondition(manager))
            && ConditionPtr(new LightModeCondition(Qn::LightModeNoNewWindow, manager))
        );

    factory(QnActions::OpenSingleLayoutAction).
        flags(Tree | SingleTarget | ResourceTarget).
        text(tr("Open Layout in New Tab")).
        condition(hasFlags(Qn::layout));

    factory(QnActions::OpenMultipleLayoutsAction).
        flags(Tree | MultiTarget | ResourceTarget).
        text(tr("Open Layouts")).
        condition(hasFlags(Qn::layout));

    factory(QnActions::OpenLayoutsInNewWindowAction).
        flags(Tree | SingleTarget | MultiTarget | ResourceTarget).
        text(tr("Open Layout(s) in New Window")). // TODO: #Elric split into sinle- & multi- action
        condition(
            condition::hasFlags(Qn::layout, All, manager)
            && ConditionPtr(new LightModeCondition(Qn::LightModeNoNewWindow, manager))
        );

    factory(QnActions::OpenCurrentLayoutInNewWindowAction).
        flags(NoTarget).
        text(tr("Open Current Layout in New Window")).
        condition(new LightModeCondition(Qn::LightModeNoNewWindow, manager));

    factory(QnActions::OpenAnyNumberOfLayoutsAction).
        flags(SingleTarget | MultiTarget | ResourceTarget).
        text(tr("Open Layout(s)")).
        condition(hasFlags(Qn::layout));

    factory(QnActions::OpenVideoWallsReviewAction).
        flags(Tree | SingleTarget | MultiTarget | ResourceTarget).
        text(tr("Open Video Wall(s)")).
        condition(hasFlags(Qn::videowall));

    factory(QnActions::OpenInFolderAction).
        flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget).
        text(tr("Open Containing Folder")).
        shortcut(lit("Ctrl+Enter")).
        shortcut(lit("Ctrl+Return")).
        autoRepeat(false).
        condition(new OpenInFolderCondition(manager));

    factory(QnActions::IdentifyVideoWallAction).
        flags(Tree | Scene | SingleTarget | MultiTarget | ResourceTarget | VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Identify")).
        autoRepeat(false).
        condition(new IdentifyVideoWallCondition(manager));

    factory(QnActions::AttachToVideoWallAction).
        flags(Tree | SingleTarget | ResourceTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Attach to Video Wall...")).
        autoRepeat(false).
        condition(
            !condition::isSafeMode(manager)
            && condition::hasFlags(Qn::videowall, Any, manager)
        );

    factory(QnActions::StartVideoWallAction).
        flags(Tree | SingleTarget | ResourceTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Switch to Video Wall mode...")).
        autoRepeat(false).
        condition(new StartVideowallCondition(manager));

    factory(QnActions::SaveVideoWallReviewAction).
        flags(Tree | SingleTarget | ResourceTarget).
        text(tr("Save Video Wall View")).
        shortcut(lit("Ctrl+S")).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        autoRepeat(false).
        condition(
            ConditionPtr(new SaveVideowallReviewCondition(false, manager))
            && !condition::isSafeMode(manager)
        );

    factory(QnActions::SaveVideowallMatrixAction).
        flags(Tree | SingleTarget | ResourceTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Save Current Matrix")).
        autoRepeat(false).
        condition(
            ConditionPtr(new NonEmptyVideowallCondition(manager))
            && !condition::isSafeMode(manager)
        );

    factory(QnActions::LoadVideowallMatrixAction).
        flags(Tree | SingleTarget | VideoWallMatrixTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        condition(!condition::isSafeMode(manager)).
        text(tr("Load Matrix"));

    factory(QnActions::DeleteVideowallMatrixAction).
        flags(Tree | SingleTarget | MultiTarget | VideoWallMatrixTarget | IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Delete")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        condition(!condition::isSafeMode(manager)).
        autoRepeat(false);

    factory().
        flags(Scene | Tree).
        separator();

    factory(QnActions::StopVideoWallAction).
        flags(Tree | SingleTarget | ResourceTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Stop Video Wall")).
        autoRepeat(false).
        condition(new RunningVideowallCondition(manager));

    factory(QnActions::ClearVideoWallScreen).
        flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Clear Screen")).
        autoRepeat(false).
        condition(new DetachFromVideoWallCondition(manager));

    factory(QnActions::SaveLayoutAction).
        flags(TitleBar | Tree | SingleTarget | ResourceTarget).
        requiredTargetPermissions(Qn::SavePermission).
        text(tr("Save Layout")).
        condition(new SaveLayoutCondition(false, manager));

    factory(QnActions::SaveLayoutAsAction).
        flags(SingleTarget | ResourceTarget).
        requiredTargetPermissions(Qn::UserResourceRole, Qn::SavePermission).    //TODO: #GDM #access check canCreateResource permission
        text(lit("If you see this string, notify me. #GDM")).
        condition(new SaveLayoutAsCondition(false, manager));

    factory(QnActions::SaveLayoutForCurrentUserAsAction).
        flags(TitleBar | Tree | SingleTarget | ResourceTarget).
        text(tr("Save Layout As...")).
        condition(new SaveLayoutAsCondition(false, manager));

    factory().
        flags(Scene | Tree).
        separator();

    factory(QnActions::DeleteVideoWallItemAction).
        flags(Tree | SingleTarget | MultiTarget | VideoWallItemTarget | IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Delete")).
        condition(!condition::isSafeMode(manager)).
        autoRepeat(false);

    factory(QnActions::MaximizeItemAction).
        flags(Scene | SingleTarget).
        text(tr("Maximize Item")).
        shortcut(lit("Enter")).
        shortcut(lit("Return")).
        autoRepeat(false).
        condition(new ItemZoomedCondition(false, manager));

    factory(QnActions::UnmaximizeItemAction).
        flags(Scene | SingleTarget).
        text(tr("Restore Item")).
        shortcut(lit("Enter")).
        shortcut(lit("Return")).
        autoRepeat(false).
        condition(new ItemZoomedCondition(true, manager));

    factory(QnActions::ShowInfoAction).
        flags(Scene | SingleTarget | MultiTarget).
        text(tr("Show Info")).
        shortcut(lit("Alt+I")).
        condition(new DisplayInfoCondition(false, manager));

    factory(QnActions::HideInfoAction).
        flags(Scene | SingleTarget | MultiTarget).
        text(tr("Hide Info")).
        shortcut(lit("Alt+I")).
        condition(new DisplayInfoCondition(true, manager));

    factory(QnActions::ToggleInfoAction).
        flags(Scene | SingleTarget | MultiTarget | HotkeyOnly).
        text(tr("Toggle Info")).
        shortcut(lit("Alt+I")).
        condition(new DisplayInfoCondition(manager));

    factory().
        flags(Scene | NoTarget).
        text(tr("Change Resolution...")).
        condition(new ChangeResolutionCondition(manager));

    factory.beginSubMenu();
    {
        factory.beginGroup();
        factory(QnActions::RadassAutoAction).
            flags(Scene | NoTarget).
            text(tr("Auto")).
            checkable().
            checked();

        factory(QnActions::RadassLowAction).
            flags(Scene | NoTarget).
            text(tr("Low")).
            checkable();

        factory(QnActions::RadassHighAction).
            flags(Scene | NoTarget).
            text(tr("High")).
            checkable();
        factory.endGroup();
    } factory.endSubMenu();

    factory().
        flags(Scene | SingleTarget).
        childFactory(new PtzPresetsToursFactory(manager)).
        text(tr("PTZ...")).
        requiredTargetPermissions(Qn::WritePtzPermission).
        condition(new PtzCondition(Qn::PresetsPtzCapability, false, manager));

    factory.beginSubMenu();
    {

        factory(QnActions::PtzSavePresetAction).
            mode(DesktopMode).
            flags(Scene | SingleTarget).
            text(tr("Save Current Position...")).
            requiredTargetPermissions(Qn::WritePtzPermission | Qn::SavePermission).
            condition(new PtzCondition(Qn::PresetsPtzCapability, true, manager));

        factory(QnActions::PtzManageAction).
            mode(DesktopMode).
            flags(Scene | SingleTarget).
            text(tr("Manage...")).
            requiredTargetPermissions(Qn::WritePtzPermission | Qn::SavePermission).
            condition(new PtzCondition(Qn::ToursPtzCapability, false, manager));

    } factory.endSubMenu();

    factory(QnActions::PtzCalibrateFisheyeAction).
        flags(SingleTarget | WidgetTarget).
        text(tr("Calibrate Fisheye")).
        condition(new PtzCondition(Qn::VirtualPtzCapability, false, manager));

#if 0
    factory(QnActions::ToggleRadassAction).
        flags(Scene | SingleTarget | MultiTarget | HotkeyOnly).
        text(tr("Toggle Resolution Mode")).
        shortcut(lit("Alt+I")).
        condition(new DisplayInfoCondition(manager));
#endif

    factory(QnActions::StartSmartSearchAction).
        flags(Scene | SingleTarget | MultiTarget).
        text(tr("Show Motion/Smart Search")).
        conditionalText(tr("Show Motion"), new NoArchiveCondition(manager)).
        shortcut(lit("Alt+G")).
        condition(new SmartSearchCondition(false, manager));

    // TODO: #ynikitenkov remove this action, use StartSmartSearchAction with checked state!
    factory(QnActions::StopSmartSearchAction).
        flags(Scene | SingleTarget | MultiTarget).
        text(tr("Hide Motion/Smart Search")).
        conditionalText(tr("Hide Motion"), new NoArchiveCondition(manager)).
        shortcut(lit("Alt+G")).
        condition(new SmartSearchCondition(true, manager));

    factory(QnActions::ClearMotionSelectionAction).
        flags(Scene | SingleTarget | MultiTarget).
        text(tr("Clear Motion Selection")).
        condition(new ClearMotionSelectionCondition(manager));

    factory(QnActions::ToggleSmartSearchAction).
        flags(Scene | SingleTarget | MultiTarget | HotkeyOnly).
        text(tr("Toggle Smart Search")).
        shortcut(lit("Alt+G")).
        condition(new SmartSearchCondition(manager));

    factory(QnActions::CheckFileSignatureAction).
        flags(Scene | SingleTarget).
        text(tr("Check File Watermark")).
        shortcut(lit("Alt+C")).
        autoRepeat(false).
        condition(new CheckFileSignatureCondition(manager));

    factory(QnActions::TakeScreenshotAction).
        flags(Scene | SingleTarget | HotkeyOnly).
        text(tr("Take Screenshot")).
        shortcut(lit("Alt+S")).
        autoRepeat(false).
        condition(new TakeScreenshotCondition(manager));

    factory(QnActions::AdjustVideoAction).
        flags(Scene | SingleTarget).
        text(tr("Image Enhancement...")).
        shortcut(lit("Alt+J")).
        autoRepeat(false).
        condition(new AdjustVideoCondition(manager));

    factory(QnActions::CreateZoomWindowAction).
        flags(SingleTarget | WidgetTarget).
        text(tr("Create Zoom Window")).
        condition(new CreateZoomWindowCondition(manager));

    factory().
        flags(Scene | SingleTarget | MultiTarget).
        text(tr("Rotate to..."));

    factory.beginSubMenu();
    {
        factory(QnActions::Rotate0Action).
            flags(Scene | SingleTarget | MultiTarget).
            text(tr("0 degrees")).
            condition(new RotateItemCondition(manager));

        factory(QnActions::Rotate90Action).
            flags(Scene | SingleTarget | MultiTarget).
            text(tr("90 degrees")).
            condition(new RotateItemCondition(manager));

        factory(QnActions::Rotate180Action).
            flags(Scene | SingleTarget | MultiTarget).
            text(tr("180 degrees")).
            condition(new RotateItemCondition(manager));

        factory(QnActions::Rotate270Action).
            flags(Scene | SingleTarget | MultiTarget).
            text(tr("270 degrees")).
            condition(new RotateItemCondition(manager));
    } factory.endSubMenu();

    factory().
        flags(Scene | Tree).
        separator();

    factory(QnActions::RemoveLayoutItemAction).
        flags(Tree | SingleTarget | MultiTarget | LayoutItemTarget | IntentionallyAmbiguous).
        text(tr("Remove from Layout")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        autoRepeat(false).
        condition(new LayoutItemRemovalCondition(manager));

    factory(QnActions::RemoveLayoutItemFromSceneAction).
        flags(Scene | SingleTarget | MultiTarget | LayoutItemTarget | IntentionallyAmbiguous).
        text(tr("Remove from Layout")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        autoRepeat(false).
        condition(new LayoutItemRemovalCondition(manager));

    factory(QnActions::RemoveFromServerAction).
        flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous).
        requiredTargetPermissions(Qn::RemovePermission).
        text(tr("Delete")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        autoRepeat(false).
        condition(new ResourceRemovalCondition(manager));

    factory(QnActions::StopSharingLayoutAction).
        flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Stop Sharing Layout")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        autoRepeat(false).
        condition(new StopSharingCondition(manager));

    factory().
        flags(Scene | Tree).
        separator();

    factory(QnActions::WebPageSettingsAction).
        flags(Scene | Tree | SingleTarget | ResourceTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Edit...")).
        autoRepeat(false).
        condition(
            condition::hasFlags(Qn::web_page, ExactlyOne, manager)
            && !condition::isSafeMode(manager)
        );

    factory(QnActions::RenameResourceAction).
        flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous).
        requiredTargetPermissions(Qn::WritePermission | Qn::WriteNamePermission).
        text(tr("Rename")).
        shortcut(lit("F2")).
        autoRepeat(false).
        condition(new RenameResourceCondition(manager));

    factory(QnActions::RenameVideowallEntityAction).
        flags(Tree | SingleTarget | VideoWallItemTarget | VideoWallMatrixTarget | IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Rename")).
        shortcut(lit("F2")).
        condition(!condition::isSafeMode(manager)).
        autoRepeat(false);

    //TODO: #GDM #3.1 #tbd
    factory(QnActions::RenameLayoutTourAction).
        flags(Tree | NoTarget | IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Rename")).
        shortcut(lit("F2")).
        condition(
            ConditionPtr(new TreeNodeTypeCondition(Qn::LayoutTourNode, manager))
            && !condition::isSafeMode(manager)
        ).
        autoRepeat(false);

    factory().
        flags(Tree | SingleTarget | ResourceTarget).
        separator();

    //TODO: #gdm restore this functionality and allow to delete exported layouts
    factory(QnActions::DeleteFromDiskAction).
        //flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget).
        text(tr("Delete from Disk")).
        autoRepeat(false).
        condition(hasFlags(Qn::local_media));

    factory(QnActions::SetAsBackgroundAction).
        flags(Scene | SingleTarget).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission).
        text(tr("Set as Layout Background")).
        autoRepeat(false).
        condition(
            ConditionPtr(new SetAsBackgroundCondition(manager))
            && ConditionPtr(new LightModeCondition(Qn::LightModeNoLayoutBackground, manager))
            && !condition::isSafeMode(manager)
        );

    factory(QnActions::UserSettingsAction).
        flags(Tree | SingleTarget | ResourceTarget).
        text(tr("User Settings...")).
        requiredTargetPermissions(Qn::ReadPermission).
        condition(hasFlags(Qn::user));

    factory(QnActions::UserRolesAction).
        flags(Tree | NoTarget).
        text(tr("User Roles...")).
        conditionalText(tr("Role Settings..."), new TreeNodeTypeCondition(Qn::RoleNode, manager)).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            ConditionPtr(new TreeNodeTypeCondition(Qn::UsersNode, manager))
            || ConditionPtr(new TreeNodeTypeCondition(Qn::RoleNode, manager))
        );

    factory(QnActions::CameraIssuesAction).
        mode(DesktopMode).
        flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget).
        dynamicText(new DevicesNameTextFactory(
            QnCameraDeviceStringSet(
                tr("Check Device Issues..."), tr("Check Devices Issues..."),
                tr("Check Camera Issues..."), tr("Check Cameras Issues..."),
                tr("Check I/O Module Issues..."), tr("Check I/O Modules Issues...")
            ), manager)).
        requiredGlobalPermission(Qn::GlobalViewLogsPermission).
        condition(
            condition::hasFlags(Qn::live_cam, Any, manager)
            && !condition::isPreviewSearchMode(manager)
        );

    factory(QnActions::CameraBusinessRulesAction).
        mode(DesktopMode).
        flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget).
        dynamicText(new DevicesNameTextFactory(
            QnCameraDeviceStringSet(
                tr("Device Rules..."), tr("Devices Rules..."),
                tr("Camera Rules..."), tr("Cameras Rules..."),
                tr("I/O Module Rules..."), tr("I/O Modules Rules...")
            ), manager)).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            condition::hasFlags(Qn::live_cam, ExactlyOne, manager)
            && !condition::isPreviewSearchMode(manager)
        );

    factory(QnActions::CameraSettingsAction).
        mode(DesktopMode).
        flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget).
        dynamicText(new DevicesNameTextFactory(
            QnCameraDeviceStringSet(
                tr("Device Settings..."), tr("Devices Settings..."),
                tr("Camera Settings..."), tr("Cameras Settings..."),
                tr("I/O Module Settings..."), tr("I/O Modules Settings...")
            ), manager)).
        requiredGlobalPermission(Qn::GlobalEditCamerasPermission).
        condition(
            condition::hasFlags(Qn::live_cam, Any, manager)
            && !condition::isPreviewSearchMode(manager)
        );

    factory(QnActions::MediaFileSettingsAction).
        mode(DesktopMode).
        flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget).
        text(tr("File Settings...")).
        condition(condition::hasFlags(Qn::local_media, Any, manager));

    factory(QnActions::LayoutSettingsAction).
        mode(DesktopMode).
        flags(Tree | SingleTarget | ResourceTarget).
        text(tr("Layout Settings...")).
        requiredTargetPermissions(Qn::EditLayoutSettingsPermission).
        condition(new LightModeCondition(Qn::LightModeNoLayoutBackground, manager));

    factory(QnActions::VideowallSettingsAction).
        flags(Tree | SingleTarget | ResourceTarget).
        text(tr("Video Wall Settings...")).
        condition(
            condition::hasFlags(Qn::videowall, ExactlyOne, manager)
            && ConditionPtr(new AutoStartAllowedCondition(manager))
            && !condition::isSafeMode(manager)
        );

    factory(QnActions::ServerAddCameraManuallyAction).
        flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget).
        text(tr("Add Device...")).   //intentionally hardcode devices here
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            condition::hasFlags(Qn::remote_server, ExactlyOne, manager)
            && ConditionPtr(new EdgeServerCondition(false, manager))
            && !ConditionPtr(new FakeServerCondition(true, manager))
            && !condition::isSafeMode(manager)
        );

    factory(QnActions::CameraListByServerAction).
        flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget).
        text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->resourcePool(),
            tr("Devices List by Server..."),
            tr("Cameras List by Server...")
        )).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            condition::hasFlags(Qn::remote_server, ExactlyOne, manager)
            && ConditionPtr(new EdgeServerCondition(false, manager))
            && !ConditionPtr(new FakeServerCondition(true, manager))
        );

    factory(QnActions::PingAction).
        flags(NoTarget).
        text(lit("Ping..."));

    factory(QnActions::ServerLogsAction).
        flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget).
        text(tr("Server Logs...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            condition::hasFlags(Qn::remote_server, ExactlyOne, manager)
            && !ConditionPtr(new FakeServerCondition(true, manager))
        );

    factory(QnActions::ServerIssuesAction).
        flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget).
        text(tr("Server Diagnostics...")).
        requiredGlobalPermission(Qn::GlobalViewLogsPermission).
        condition(
            condition::hasFlags(Qn::remote_server, ExactlyOne, manager)
            && !ConditionPtr(new FakeServerCondition(true, manager))
        );

    factory(QnActions::WebAdminAction).
        flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget).
        text(tr("Server Web Page...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            condition::hasFlags(Qn::remote_server, ExactlyOne, manager)
            && !ConditionPtr(new FakeServerCondition(true, manager))
            && !ConditionPtr(new CloudServerCondition(Any, manager))
        );

    factory(QnActions::ServerSettingsAction).
        flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget).
        text(tr("Server Settings...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            condition::hasFlags(Qn::remote_server, ExactlyOne, manager)
            && !ConditionPtr(new FakeServerCondition(true, manager))
        );

    factory(QnActions::ConnectToCurrentSystem).
        flags(Tree | SingleTarget | MultiTarget | ResourceTarget).
        text(tr("Merge to Currently Connected System...")).
        condition(
            ConditionPtr(new TreeNodeTypeCondition(Qn::ResourceNode, manager))
            && !condition::isSafeMode(manager)
            && ConditionPtr(new MergeToCurrentSystemCondition(manager))
            && ConditionPtr(new RequiresOwnerCondition(manager))
        );

    factory().
        flags(Scene | NoTarget).
        childFactory(new AspectRatioFactory(manager)).
        text(tr("Change Cell Aspect Ratio...")).
        condition(
            !ConditionPtr(new VideoWallReviewModeCondition(manager))
            && ConditionPtr(new LightModeCondition(Qn::LightModeSingleItem, manager))
        );

    factory().
        flags(Scene | NoTarget).
        text(tr("Change Cell Spacing...")).
        condition(new LightModeCondition(Qn::LightModeSingleItem, manager));

    factory.beginSubMenu();
    {
        factory.beginGroup();

        factory(QnActions::SetCurrentLayoutItemSpacingNoneAction).
            flags(Scene | NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("None")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::None));

        factory(QnActions::SetCurrentLayoutItemSpacingSmallAction).
            flags(Scene | NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Small")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Small));

        factory(QnActions::SetCurrentLayoutItemSpacingMediumAction).
            flags(Scene | NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Medium")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Medium));

        factory(QnActions::SetCurrentLayoutItemSpacingLargeAction).
            flags(Scene | NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Large")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Large));
        factory.endGroup();

    } factory.endSubMenu();

    factory().
        flags(Scene | NoTarget).
        separator();

    factory(QnActions::ReviewLayoutTourAction).
        flags(Tree | NoTarget).
        mode(DesktopMode).
        text(tr("Review Layout Tour")).
        condition(new TreeNodeTypeCondition(Qn::LayoutTourNode, manager)).
        autoRepeat(false);

    factory(QnActions::ToggleLayoutTourModeAction).
        flags(Scene | Tree | NoTarget | GlobalHotkey).
        mode(DesktopMode).
        text(tr("Start Tour")).
        shortcut(lit("Alt+T")).
        checkable().
        autoRepeat(false).
        condition(
            ConditionPtr(new TreeNodeTypeCondition(Qn::LayoutTourNode, manager))
            && ConditionPtr(new ToggleTourCondition(manager))
        );

    factory(QnActions::RemoveLayoutTourAction).
        flags(Tree | NoTarget | IntentionallyAmbiguous).
        mode(DesktopMode).
        text(tr("Delete Layout Tour")).
        requiredGlobalPermission(Qn::GlobalAdminPermission). //TODO: #GDM #3.1 #tbd
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        condition(new TreeNodeTypeCondition(Qn::LayoutTourNode, manager));

    factory(QnActions::StartCurrentLayoutTourAction).
        flags(Scene | NoTarget).
        mode(DesktopMode).
        text(tr("Start Tour")).
        accent(Qn::ButtonAccent::Standard).
        icon(qnSkin->icon("buttons/play.png")).
        condition(
            ConditionPtr(new LayoutTourReviewModeCondition(manager))
            && ConditionPtr(new StartCurrentLayoutTourCondition(manager))
        ).
        autoRepeat(false);

    factory(QnActions::SaveLayoutTourAction).
        flags(NoTarget).
        text(lit("Save layout tour (internal)")).
        mode(DesktopMode);

    factory(QnActions::SaveCurrentLayoutTourAction).
        flags(Scene | NoTarget).
        mode(DesktopMode).
        text(tr("Save Changes")).
        condition(new LayoutTourReviewModeCondition(manager)).
        autoRepeat(false);

    factory(QnActions::RemoveCurrentLayoutTourAction).
        flags(NoTarget).
        mode(DesktopMode).
        icon(qnSkin->icon("buttons/delete.png")).
        condition(new LayoutTourReviewModeCondition(manager)).
        autoRepeat(false);

    factory().
        flags(Scene | NoTarget).
        separator();

    factory(QnActions::CurrentLayoutSettingsAction).
        flags(Scene | NoTarget).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission).
        text(tr("Layout Settings...")).
        condition(new LightModeCondition(Qn::LightModeNoLayoutBackground, manager));

    /* Tab bar actions. */
    factory().
        flags(TitleBar).
        separator();

    factory(QnActions::CloseLayoutAction).
        flags(TitleBar | ScopelessHotkey | SingleTarget).
        mode(DesktopMode).
        text(tr("Close")).
        shortcut(lit("Ctrl+W")).
        autoRepeat(false);

    factory(QnActions::CloseAllButThisLayoutAction).
        flags(TitleBar | SingleTarget).
        mode(DesktopMode).
        text(tr("Close All But This")).
        condition(new LayoutCountCondition(2, manager));

    /* Slider actions. */
    factory(QnActions::StartTimeSelectionAction).
        flags(Slider | SingleTarget).
        text(tr("Mark Selection Start")).
        shortcut(lit("[")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new TimePeriodCondition(NullTimePeriod, InvisibleAction, manager));

    factory(QnActions::EndTimeSelectionAction).
        flags(Slider | SingleTarget).
        text(tr("Mark Selection End")).
        shortcut(lit("]")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new TimePeriodCondition(EmptyTimePeriod, InvisibleAction, manager));

    factory(QnActions::ClearTimeSelectionAction).
        flags(Slider | SingleTarget).
        text(tr("Clear Selection")).
        condition(new TimePeriodCondition(EmptyTimePeriod | NormalTimePeriod, InvisibleAction, manager));

    factory(QnActions::ZoomToTimeSelectionAction).
        flags(Slider | SingleTarget).
        text(tr("Zoom to Selection")).
        condition(new TimePeriodCondition(NormalTimePeriod, InvisibleAction, manager));

    factory(QnActions::AddCameraBookmarkAction).
        flags(Slider | SingleTarget).
        text(tr("Add Bookmark...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(
            !condition::isSafeMode(manager)
            && ConditionPtr(new AddBookmarkCondition(manager))
        );

    factory(QnActions::EditCameraBookmarkAction).
        flags(Slider | SingleTarget | ResourceTarget).
        text(tr("Edit Bookmark...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(
            !condition::isSafeMode(manager)
            && ConditionPtr(new ModifyBookmarkCondition(manager))
        );

    factory(QnActions::RemoveCameraBookmarkAction).
        flags(Slider | SingleTarget).
        text(tr("Remove Bookmark...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(
            !condition::isSafeMode(manager)
            && ConditionPtr(new ModifyBookmarkCondition(manager))
        );

    factory(QnActions::RemoveBookmarksAction).
        flags(NoTarget | SingleTarget | ResourceTarget).
        text(tr("Remove Bookmarks...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(
            !condition::isSafeMode(manager)
            && ConditionPtr(new RemoveBookmarksCondition(manager))
        );

    factory().
        flags(Slider | SingleTarget).
        separator();

    factory(QnActions::ExportTimeSelectionAction).
        flags(Slider | SingleTarget | ResourceTarget).
        text(tr("Export Selected Area...")).
        requiredTargetPermissions(Qn::ExportPermission).
        condition(new ExportCondition(true, manager));

    factory(QnActions::ExportLayoutAction).
        flags(Slider | SingleTarget | MultiTarget | NoTarget).
        text(tr("Export Multi-Video...")).
        requiredTargetPermissions(Qn::CurrentLayoutMediaItemsRole, Qn::ExportPermission).
        condition(new ExportCondition(false, manager));

    factory(QnActions::ExportTimelapseAction).
        flags(Slider | SingleTarget | MultiTarget | NoTarget).
        text(tr("Export Rapid Review...")).
        requiredTargetPermissions(Qn::CurrentLayoutMediaItemsRole, Qn::ExportPermission).
        condition(new ExportCondition(true, manager));

    factory(QnActions::ThumbnailsSearchAction).
        flags(Slider | Scene | SingleTarget).
        mode(DesktopMode).
        text(tr("Preview Search...")).
        condition(new PreviewCondition(manager));


    factory(QnActions::DebugIncrementCounterAction).
        flags(GlobalHotkey | DevMode).
        shortcut(lit("Ctrl+Alt+Shift++")).
        text(lit("Increment Debug Counter"));

    factory(QnActions::DebugDecrementCounterAction).
        flags(GlobalHotkey | DevMode).
        shortcut(lit("Ctrl+Alt+Shift+-")).
        text(lit("Decrement Debug Counter"));

    factory(QnActions::DebugCalibratePtzAction).
        flags(Scene | SingleTarget | DevMode).
        text(lit("Calibrate PTZ"));

    factory(QnActions::DebugGetPtzPositionAction).
        flags(Scene | SingleTarget | DevMode).
        text(lit("Get PTZ Position"));

    factory(QnActions::DebugControlPanelAction).
        flags(GlobalHotkey | DevMode).
        shortcut(lit("Ctrl+Alt+Shift+D")).
        text(lit("Debug Control Panel"));

    factory(QnActions::PlayPauseAction).
        flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget).
        shortcut(lit("Space")).
        text(tr("Play")).
        toggledText(tr("Pause")).
        condition(new ArchiveCondition(manager));

    factory(QnActions::PreviousFrameAction).
        flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget).
        shortcut(lit("Ctrl+Left")).
        text(tr("Previous Frame")).
        condition(new ArchiveCondition(manager));

    factory(QnActions::NextFrameAction).
        flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget).
        shortcut(lit("Ctrl+Right")).
        text(tr("Next Frame")).
        condition(new ArchiveCondition(manager));

    factory(QnActions::JumpToStartAction).
        flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget).
        shortcut(lit("Z")).
        text(tr("To Start")).
        condition(new ArchiveCondition(manager));

    factory(QnActions::JumpToEndAction).
        flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget).
        shortcut(lit("X")).
        text(tr("To End")).
        condition(new ArchiveCondition(manager));

    factory(QnActions::VolumeUpAction).
        flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget).
        shortcut(lit("Ctrl+Up")).
        text(tr("Volume Down")).
        condition(new TimelineVisibleCondition(manager));

    factory(QnActions::VolumeDownAction).
        flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget).
        shortcut(lit("Ctrl+Down")).
        text(tr("Volume Up")).
        condition(new TimelineVisibleCondition(manager));

    factory(QnActions::ToggleMuteAction).
        flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget).
        shortcut(lit("M")).
        text(tr("Toggle Mute")).
        checkable().
        condition(new TimelineVisibleCondition(manager));

    factory(QnActions::JumpToLiveAction).
        flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget).
        shortcut(lit("L")).
        text(tr("Jump to Live")).
        checkable().
        condition(new ArchiveCondition(manager));

    factory(QnActions::ToggleSyncAction).
        flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget).
        shortcut(lit("S")).
        text(tr("Synchronize Streams")).
        toggledText(tr("Disable Stream Synchronization")).
        condition(new ArchiveCondition(manager));


    factory().
        flags(Slider | TitleBar | Tree).
        separator();

    factory(QnActions::ToggleThumbnailsAction).
        flags(NoTarget).
        text(tr("Show Thumbnails")).
        toggledText(tr("Hide Thumbnails"));

    factory(QnActions::BookmarksModeAction).
        flags(NoTarget).
        text(tr("Show Bookmarks")).
        requiredGlobalPermission(Qn::GlobalViewBookmarksPermission).
        toggledText(tr("Hide Bookmarks"));

    factory(QnActions::ToggleCalendarAction).
        flags(NoTarget).
        text(tr("Show Calendar")).
        toggledText(tr("Hide Calendar"));

    factory(QnActions::ToggleTitleBarAction).
        flags(NoTarget).
        text(tr("Show Title Bar")).
        toggledText(tr("Hide Title Bar")).
        condition(new ToggleTitleBarCondition(manager));

    factory(QnActions::PinTreeAction).
        flags(Tree | NoTarget).
        text(tr("Pin Tree")).
        toggledText(tr("Unpin Tree")).
        condition(new TreeNodeTypeCondition(Qn::RootNode, manager));

    factory(QnActions::PinCalendarAction).
        text(tr("Pin Calendar")).
        toggledText(tr("Unpin Calendar"));

    factory(QnActions::MinimizeDayTimeViewAction).
        text(tr("Minimize")).
        icon(qnSkin->icon("titlebar/dropdown.png"));

    factory(QnActions::ToggleTreeAction).
        flags(NoTarget).
        text(tr("Show Tree")).
        toggledText(tr("Hide Tree")).
        condition(new TreeNodeTypeCondition(Qn::RootNode, manager));

    factory(QnActions::ToggleTimelineAction).
        flags(NoTarget).
        text(tr("Show Timeline")).
        toggledText(tr("Hide Timeline"));

    factory(QnActions::ToggleNotificationsAction).
        flags(NoTarget).
        text(tr("Show Notifications")).
        toggledText(tr("Hide Notifications"));

    factory(QnActions::PinNotificationsAction).
        flags(Notifications | NoTarget).
        text(tr("Pin Notifications")).
        toggledText(tr("Unpin Notifications"));

    factory(QnActions::GoToNextItemAction)
        .flags(NoTarget);

    factory(QnActions::GoToPreviousItemAction)
        .flags(NoTarget);

    factory(QnActions::ToggleCurrentItemMaximizationStateAction)
        .flags(NoTarget);

    factory(QnActions::PtzContinuousMoveAction)
        .flags(NoTarget);

    factory(QnActions::PtzActivatePresetByIndexAction)
        .flags(NoTarget);
}
