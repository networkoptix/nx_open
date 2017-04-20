#include "actions.h"

#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_criterion.h>

#include <client/client_runtime_settings.h>

#include <nx/client/desktop/ui/actions/menu_factory.h>
#include <ui/actions/action_conditions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_factories.h>
#include <ui/actions/action_text_factories.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/fusion/model_functions.h>

#include <nx/network/app_info.h>

#include <nx/utils/app_info.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnActions, IDType)

void QnActions::initialize(QnActionManager* manager, QnAction* root)
{
    using namespace nx::client::desktop::ui::action;

    MenuFactory factory(manager, root);

    using namespace QnResourceCriterionExpressions;

    /* Actions that are not assigned to any menu. */

    factory(QnActions::ShowFpsAction).
        flags(Qn::GlobalHotkey).
        text(lit("Show FPS")).
        shortcut(lit("Ctrl+Alt+F")).
        autoRepeat(false);

    factory(QnActions::ShowDebugOverlayAction).
        flags(Qn::GlobalHotkey).
        text(lit("Show Debug")).
        shortcut(lit("Ctrl+Alt+D")).
        autoRepeat(false);

    factory(QnActions::DropResourcesAction).
        flags(Qn::ResourceTarget | Qn::WidgetTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::SingleTarget | Qn::MultiTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Drop Resources"));

    factory(QnActions::DropResourcesIntoNewLayoutAction).
        flags(Qn::ResourceTarget | Qn::WidgetTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::SingleTarget | Qn::MultiTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Drop Resources into New Layout"));

    factory(QnActions::DelayedOpenVideoWallItemAction).
        flags(Qn::NoTarget).
        text(tr("Delayed Open Video Wall"));

    factory(QnActions::DelayedDropResourcesAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Delayed Drop Resources"));

    factory(QnActions::InstantDropResourcesAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Instant Drop Resources"));

    factory(QnActions::MoveCameraAction).
        flags(Qn::ResourceTarget | Qn::SingleTarget | Qn::MultiTarget).
        requiredTargetPermissions(Qn::RemovePermission).
        text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->resourcePool(),
            tr("Move Devices"),
            tr("Move Cameras")
        )).
        condition(hasFlags(Qn::network));

    factory(QnActions::NextLayoutAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Next Layout")).
        shortcut(lit("Ctrl+Tab")).
        autoRepeat(false);

    factory(QnActions::PreviousLayoutAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Previous Layout")).
        shortcut(lit("Ctrl+Shift+Tab")).
        autoRepeat(false);

    factory(QnActions::SelectAllAction).
        flags(Qn::GlobalHotkey).
        text(tr("Select All")).
        shortcut(lit("Ctrl+A")).
        shortcutContext(Qt::WidgetWithChildrenShortcut).
        autoRepeat(false);

    factory(QnActions::SelectionChangeAction).
        flags(Qn::NoTarget).
        text(tr("Selection Changed"));

    factory(QnActions::PreferencesLicensesTabAction).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::PreferencesSmtpTabAction).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::PreferencesNotificationTabAction).
        flags(Qn::NoTarget).
        icon(qnSkin->icon("events/filter.png")).
        text(tr("Filter..."));

    factory(QnActions::PreferencesCloudTabAction).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::ConnectAction).
        flags(Qn::NoTarget);

    factory(QnActions::ConnectToCloudSystemAction).
        flags(Qn::Tree | Qn::NoTarget).
        text(tr("Connect to System")).
        condition(new QnTreeNodeTypeCondition(Qn::CloudSystemNode, manager));

    factory(QnActions::ReconnectAction).
        flags(Qn::NoTarget);

    factory(QnActions::FreespaceAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Go to Freespace Mode")).
        shortcut(lit("F11")).
        autoRepeat(false);

    factory(QnActions::WhatsThisAction).
        flags(Qn::NoTarget).
        text(tr("Help")).
        icon(qnSkin->icon("titlebar/window_question.png"));

    factory(QnActions::CameraDiagnosticsAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::ResourceTarget | Qn::SingleTarget).
        dynamicText(new QnDevicesNameActionTextFactory(
            QnCameraDeviceStringSet(
                tr("Device Diagnostics..."),
                tr("Camera Diagnostics..."),
                tr("I/O Module Diagnostics...")
            ), manager)).
        condition(new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::Any, manager));

    factory(QnActions::OpenBusinessLogAction).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget
            | Qn::LayoutItemTarget | Qn::WidgetTarget | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        requiredGlobalPermission(Qn::GlobalViewLogsPermission).
        icon(qnSkin->icon("events/log.png")).
        shortcut(lit("Ctrl+L")).
        text(tr("Event Log..."));

    factory(QnActions::OpenBusinessRulesAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Event Rules..."));

    factory(QnActions::OpenFailoverPriorityAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Failover Priority..."));

    factory(QnActions::OpenBackupCamerasAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Cameras to Backup..."));

    factory(QnActions::StartVideoWallControlAction).
        flags(Qn::Tree | Qn::VideoWallReviewScene | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Control Video Wall")).
        condition(
            QnActionConditionPtr(new QnStartVideoWallControlActionCondition(manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
        );

    factory(QnActions::PushMyScreenToVideowallAction).
        flags(Qn::Tree | Qn::VideoWallReviewScene | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Push my screen")).
        condition(
            QnActionConditionPtr(new QnDesktopCameraActionCondition(manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
        );

    factory(QnActions::QueueAppRestartAction).
        flags(Qn::NoTarget).
        text(tr("Restart application"));

    factory(QnActions::SelectTimeServerAction).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Select Time Server"));

    factory(QnActions::PtzActivatePresetAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Go To Saved Position")).
        requiredTargetPermissions(Qn::WritePtzPermission).
        condition(new QnPtzActionCondition(Qn::PresetsPtzCapability, false, manager));

    factory(QnActions::PtzActivateTourAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Activate PTZ Tour")).
        requiredTargetPermissions(Qn::WritePtzPermission).
        condition(new QnPtzActionCondition(Qn::ToursPtzCapability, false, manager));

    factory(QnActions::PtzActivateObjectAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Activate PTZ Object")).
        requiredTargetPermissions(Qn::WritePtzPermission);

    /* Context menu actions. */

    factory(QnActions::FitInViewAction).
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Fit in View"));

    factory().
        flags(Qn::Scene).
        separator();

    factory(QnActions::MainMenuAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Main Menu")).
        shortcut(lit("Alt+Space"), Builder::Mac, true).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/main_menu.png"));

    factory(QnActions::OpenLoginDialogAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Connect to Server...")).
        shortcut(lit("Ctrl+Shift+C")).
        autoRepeat(false);

    factory(QnActions::DisconnectAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Disconnect from Server")).
        autoRepeat(false).
        shortcut(lit("Ctrl+Shift+D")).
        condition(new QnLoggedInCondition(manager));

    factory(QnActions::ResourcesModeAction).
        flags(Qn::Main).
        mode(QnActionTypes::DesktopMode).
        text(tr("Browse Local Files")).
        toggledText(tr("Show Welcome Screen")).
        condition(new QnBrowseLocalFilesCondition(manager));

    factory().
        flags(Qn::Main).
        separator();

    factory(QnActions::TogglePanicModeAction).
        flags(Qn::GlobalHotkey | Qn::DevMode).
        mode(QnActionTypes::DesktopMode).
        text(tr("Start Panic Recording")).
        toggledText(tr("Stop Panic Recording")).
        autoRepeat(false).
        shortcut(lit("Ctrl+P")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnPanicActionCondition(manager));

    factory().
        flags(Qn::Main | Qn::Tree).
        separator();

    factory().
        flags(Qn::Main | Qn::TitleBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("New..."));

    factory.beginSubMenu();
    {
        factory(QnActions::OpenNewTabAction).
            flags(Qn::Main | Qn::TitleBar | Qn::SingleTarget | Qn::NoTarget | Qn::GlobalHotkey).
            mode(QnActionTypes::DesktopMode).
            text(tr("Tab")).
            pulledText(tr("New Tab")).
            shortcut(lit("Ctrl+T")).
            autoRepeat(false). /* Technically, it should be auto-repeatable, but we don't want the user opening 100500 layouts and crashing the client =). */
            icon(qnSkin->icon("titlebar/new_layout.png"));

        factory(QnActions::OpenNewWindowAction).
            flags(Qn::Main | Qn::GlobalHotkey).
            mode(QnActionTypes::DesktopMode).
            text(tr("Window")).
            pulledText(tr("New Window")).
            shortcut(lit("Ctrl+N")).
            autoRepeat(false).
            condition(new QnLightModeCondition(Qn::LightModeNoNewWindow, manager));

        factory().
            flags(Qn::Main).
            separator();

        factory(QnActions::NewUserAction).
            flags(Qn::Main | Qn::Tree).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("User...")).
            pulledText(tr("New User...")).
            condition(
                QnActionConditionPtr(new QnTreeNodeTypeCondition(Qn::UsersNode, manager))
                && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            ).
            autoRepeat(false);

        factory(QnActions::NewVideoWallAction).
            flags(Qn::Main).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("Video Wall...")).
            pulledText(tr("New Video Wall...")).
            condition(new QnForbiddenInSafeModeCondition(manager)).
            autoRepeat(false);

        factory(QnActions::NewWebPageAction).
            flags(Qn::Main | Qn::Tree).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("Web Page...")).
            pulledText(tr("New Web Page...")).
            condition(
                QnActionConditionPtr(new QnTreeNodeTypeCondition(Qn::WebPagesNode, manager))
                && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            ).
            autoRepeat(false);

        factory(QnActions::NewLayoutTourAction).
            flags(Qn::Main | Qn::Tree | Qn::NoTarget).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("Layout Tour...")).
            pulledText(tr("New Layout Tour...")).
            condition(
            (
                QnActionConditionPtr(new QnTreeNodeTypeCondition(Qn::LayoutsNode, manager))
                || QnActionConditionPtr(new QnTreeNodeTypeCondition(Qn::LayoutToursNode, manager))
                )
                && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            ).
            autoRepeat(false);

    }
    factory.endSubMenu();

    factory(QnActions::NewUserLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::NoTarget).
        text(tr("New Layout...")).
        condition(
            QnActionConditionPtr(new QnNewUserLayoutActionCondition(manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
        );

    factory(QnActions::OpenCurrentUserLayoutMenu).
        flags(Qn::TitleBar | Qn::SingleTarget | Qn::NoTarget).
        text(tr("Open Layout...")).
        childFactory(new QnOpenCurrentUserLayoutActionFactory(manager)).
        icon(qnSkin->icon("titlebar/dropdown.png"));

    factory().
        flags(Qn::TitleBar).
        separator();

    factory().
        flags(Qn::Main | Qn::Tree | Qn::Scene).
        mode(QnActionTypes::DesktopMode).
        text(tr("Open..."));

    factory.beginSubMenu();
    {
        factory(QnActions::OpenFileAction).
            flags(Qn::Main | Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey).
            mode(QnActionTypes::DesktopMode).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
            text(tr("File(s)...")).
            shortcut(lit("Ctrl+O")).
            autoRepeat(false);

        factory(QnActions::OpenFolderAction).
            flags(Qn::Main | Qn::Scene).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
            text(tr("Folder..."));

        factory().separator().
            flags(Qn::Main);

        factory(QnActions::WebClientAction).
            flags(Qn::Main | Qn::Tree | Qn::NoTarget).
            text(tr("Web Client...")).
            pulledText(tr("Open Web Client...")).
            autoRepeat(false).
            condition(
                QnActionConditionPtr(new QnLoggedInCondition(manager))
                && QnActionConditionPtr(new QnTreeNodeTypeCondition({Qn::CurrentSystemNode, Qn::ServersNode}, manager))
            );

    } factory.endSubMenu();

    factory(QnActions::SaveCurrentLayoutAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey | Qn::IntentionallyAmbiguous).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::SavePermission).
        text(tr("Save Current Layout")).
        shortcut(lit("Ctrl+S")).
        autoRepeat(false). /* There is no point in saving the same layout many times in a row. */
        condition(new QnSaveLayoutActionCondition(true, manager));

    factory(QnActions::SaveCurrentLayoutAsAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey).
        text(tr("Save Current Layout As...")).
        shortcut(lit("Ctrl+Shift+S")).
        shortcut(lit("Ctrl+Alt+S"), Builder::Windows, true).
        autoRepeat(false).
        condition(
            QnActionConditionPtr(new QnLoggedInCondition(manager))
            && QnActionConditionPtr(new QnSaveLayoutAsActionCondition(true, manager))
        );

    factory(QnActions::ShareLayoutAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::SingleTarget | Qn::ResourceTarget).
        text(lit("Share_Layout_with")).
        autoRepeat(false).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnForbiddenInSafeModeCondition(manager));

    factory(QnActions::SaveCurrentVideoWallReviewAction).
        flags(Qn::Main | Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey | Qn::IntentionallyAmbiguous).
        mode(QnActionTypes::DesktopMode).
        text(tr("Save Video Wall View")).
        shortcut(lit("Ctrl+S")).
        autoRepeat(false).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        condition(
            QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            && QnActionConditionPtr(new QnSaveVideowallReviewActionCondition(true, manager))
        );

    factory(QnActions::DropOnVideoWallItemAction).
        flags(Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::VideoWallItemTarget | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Drop Resources")).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        condition(new QnForbiddenInSafeModeCondition(manager));

    factory().
        flags(Qn::Main).
        separator();

    const bool screenRecordingSupported = nx::utils::AppInfo::isWindows();
    if (screenRecordingSupported && qnRuntime->isDesktopMode())
    {
        factory(QnActions::ToggleScreenRecordingAction).
            flags(Qn::Main | Qn::GlobalHotkey).
            mode(QnActionTypes::DesktopMode).
            text(tr("Start Screen Recording")).
            toggledText(tr("Stop Screen Recording")).
            shortcut(lit("Alt+R")).
            shortcut(Qt::Key_MediaRecord).
            shortcutContext(Qt::ApplicationShortcut).
            autoRepeat(false);

        factory().
            flags(Qn::Main).
            separator();
    }

    factory(QnActions::EscapeHotkeyAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        autoRepeat(false).
        shortcut(lit("Esc")).
        text(tr("Stop current action"));

    factory(QnActions::FullscreenAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Go to Fullscreen")).
        toggledText(tr("Exit Fullscreen")).
        icon(qnSkin->icon("titlebar/window_maximize.png", "titlebar/window_restore.png"));


    factory(QnActions::MinimizeAction).
        flags(Qn::NoTarget).
        text(tr("Minimize")).
        icon(qnSkin->icon("titlebar/window_minimize.png"));

    factory(QnActions::MaximizeAction).
        flags(Qn::NoTarget).
        text(tr("Maximize")).
        toggledText(tr("Restore Down")).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/window_maximize.png", "titlebar/window_restore.png"));


    factory(QnActions::FullscreenMaximizeHotkeyAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        autoRepeat(false).
        shortcut(lit("Alt+Enter")).
        shortcut(lit("Alt+Return")).
        shortcut(lit("Ctrl+F"), Builder::Mac, true).
        shortcutContext(Qt::ApplicationShortcut);

    factory(QnActions::VersionMismatchMessageAction).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(lit("Show Version Mismatch Message"));

    factory(QnActions::BetaVersionMessageAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(lit("Show Beta Version Warning Message"));

    factory(QnActions::AllowStatisticsReportMessageAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(lit("Ask About Statistics Reporting"));

    factory(QnActions::BrowseUrlAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Open in Browser..."));

    factory(QnActions::SystemAdministrationAction).
        flags(Qn::Main | Qn::Tree | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("System Administration...")).
        shortcut(lit("Ctrl+Alt+A")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnTreeNodeTypeCondition({Qn::CurrentSystemNode, Qn::ServersNode}, manager));

    factory(QnActions::SystemUpdateAction).
        flags(Qn::NoTarget).
        text(tr("System Update...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::UserManagementAction).
        flags(Qn::Main | Qn::Tree).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("User Management...")).
        condition(new QnTreeNodeTypeCondition(Qn::UsersNode, manager));

    factory(QnActions::PreferencesGeneralTabAction).
        flags(Qn::Main).
        text(tr("Local Settings...")).
        //shortcut(lit("Ctrl+P")).
        role(QAction::PreferencesRole).
        autoRepeat(false);

    factory(QnActions::OpenAuditLogAction).
        flags(Qn::Main).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Audit Trail..."));

    factory(QnActions::OpenBookmarksSearchAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        requiredGlobalPermission(Qn::GlobalViewBookmarksPermission).
        text(tr("Bookmark Search...")).
        shortcut(lit("Ctrl+B")).
        autoRepeat(false);

    factory(QnActions::LoginToCloud).
        flags(Qn::NoTarget).
        text(tr("Log in to %1...", "Log in to Nx Cloud").arg(nx::network::AppInfo::cloudName()));

    factory(QnActions::LogoutFromCloud).
        flags(Qn::NoTarget).
        text(tr("Log out from %1", "Log out from Nx Cloud").arg(nx::network::AppInfo::cloudName()));

    factory(QnActions::OpenCloudMainUrl).
        flags(Qn::NoTarget).
        text(tr("Open %1 Portal...", "Open Nx Cloud Portal").arg(nx::network::AppInfo::cloudName()));

    factory(QnActions::OpenCloudManagementUrl).
        flags(Qn::NoTarget).
        text(tr("Account Settings..."));

    factory(QnActions::HideCloudPromoAction).
        flags(Qn::NoTarget);

    factory(QnActions::OpenCloudRegisterUrl).
        flags(Qn::NoTarget).
        text(tr("Create Account..."));

    factory().
        flags(Qn::Main).
        separator();

    factory(QnActions::BusinessEventsAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Event Rules...")).
        icon(qnSkin->icon("events/settings.png")).
        shortcut(lit("Ctrl+E")).
        autoRepeat(false);

    factory(QnActions::CameraListAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->resourcePool(),
            tr("Devices List"),
            tr("Cameras List")
        )).
        shortcut(lit("Ctrl+M")).
        autoRepeat(false);

    factory(QnActions::MergeSystems).
        flags(Qn::Main | Qn::Tree).
        text(tr("Merge Systems...")).
        condition(
            QnActionConditionPtr(new QnTreeNodeTypeCondition({Qn::CurrentSystemNode, Qn::ServersNode}, manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            && QnActionConditionPtr(new QnRequiresOwnerCondition(manager))
        );

    factory().
        flags(Qn::Main).
        separator();

    factory(QnActions::AboutAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("About...")).
        shortcut(lit("F1")).
        shortcutContext(Qt::ApplicationShortcut).
        role(QAction::AboutRole).
        autoRepeat(false);

    factory().
        flags(Qn::Main).
        separator();

    factory(QnActions::ExitAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        text(tr("Exit")).
        shortcut(lit("Alt+F4")).
        shortcut(lit("Ctrl+Q"), Builder::Mac, true).
        shortcutContext(Qt::ApplicationShortcut).
        role(QAction::QuitRole).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/window_close.png")).
        iconVisibleInMenu(false);

    factory(QnActions::DelayedForcedExitAction).
        flags(Qn::NoTarget);

    factory(QnActions::BeforeExitAction).
        flags(Qn::NoTarget);

    factory().
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        childFactory(new QnEdgeNodeActionFactory(manager)).
        text(tr("Server...")).
        condition(new QnTreeNodeTypeCondition(Qn::EdgeNode, manager));

    factory().
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        separator().
        condition(new QnTreeNodeTypeCondition(Qn::EdgeNode, manager));

    /* Resource actions. */
    factory(QnActions::OpenInLayoutAction)
        .flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget)
        .requiredTargetPermissions(Qn::LayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
        .text(tr("Open in Layout"))
        .condition(new QnOpenInLayoutActionCondition(manager));

    factory(QnActions::OpenInCurrentLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
        text(tr("Open")).
        conditionalText(tr("Monitor"), hasFlags(Qn::server), Qn::All).
        condition(new QnOpenInCurrentLayoutActionCondition(manager));

    factory(QnActions::OpenInNewLayoutAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in New Tab")).
        conditionalText(tr("Monitor in New Tab"), hasFlags(Qn::server), Qn::All).
        condition(new QnOpenInNewEntityActionCondition(manager));

    factory(QnActions::OpenInAlarmLayoutAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open in Alarm Layout"));

    factory(QnActions::OpenInNewWindowAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in New Window")).
        conditionalText(tr("Monitor in New Window"), hasFlags(Qn::server), Qn::All).
        condition(
            QnActionConditionPtr(new QnOpenInNewEntityActionCondition(manager))
            && QnActionConditionPtr(new QnLightModeCondition(Qn::LightModeNoNewWindow, manager))
        );

    factory(QnActions::OpenSingleLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Open Layout in New Tab")).
        condition(hasFlags(Qn::layout));

    factory(QnActions::OpenMultipleLayoutsAction).
        flags(Qn::Tree | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layouts")).
        condition(hasFlags(Qn::layout));

    factory(QnActions::OpenLayoutsInNewWindowAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layout(s) in New Window")). // TODO: #Elric split into sinle- & multi- action
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::layout), Qn::All, manager))
            && QnActionConditionPtr(new QnLightModeCondition(Qn::LightModeNoNewWindow, manager))
        );

    factory(QnActions::OpenCurrentLayoutInNewWindowAction).
        flags(Qn::NoTarget).
        text(tr("Open Current Layout in New Window")).
        condition(new QnLightModeCondition(Qn::LightModeNoNewWindow, manager));

    factory(QnActions::OpenAnyNumberOfLayoutsAction).
        flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layout(s)")).
        condition(hasFlags(Qn::layout));

    factory(QnActions::OpenVideoWallsReviewAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Video Wall(s)")).
        condition(hasFlags(Qn::videowall));

    factory(QnActions::OpenInFolderAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Open Containing Folder")).
        shortcut(lit("Ctrl+Enter")).
        shortcut(lit("Ctrl+Return")).
        autoRepeat(false).
        condition(new QnOpenInFolderActionCondition(manager));

    factory(QnActions::IdentifyVideoWallAction).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Identify")).
        autoRepeat(false).
        condition(new QnIdentifyVideoWallActionCondition(manager));

    factory(QnActions::AttachToVideoWallAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Attach to Video Wall...")).
        autoRepeat(false).
        condition(
            QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            && QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::videowall), Qn::Any, manager))
        );

    factory(QnActions::StartVideoWallAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Switch to Video Wall mode...")).
        autoRepeat(false).
        condition(new QnStartVideowallActionCondition(manager));

    factory(QnActions::SaveVideoWallReviewAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Save Video Wall View")).
        shortcut(lit("Ctrl+S")).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        autoRepeat(false).
        condition(
            QnActionConditionPtr(new QnSaveVideowallReviewActionCondition(false, manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
        );

    factory(QnActions::SaveVideowallMatrixAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Save Current Matrix")).
        autoRepeat(false).
        condition(
            QnActionConditionPtr(new QnNonEmptyVideowallActionCondition(manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
        );

    factory(QnActions::LoadVideowallMatrixAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::VideoWallMatrixTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        condition(new QnForbiddenInSafeModeCondition(manager)).
        text(tr("Load Matrix"));

    factory(QnActions::DeleteVideowallMatrixAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallMatrixTarget | Qn::IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Delete")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        condition(new QnForbiddenInSafeModeCondition(manager)).
        autoRepeat(false);

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(QnActions::StopVideoWallAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Stop Video Wall")).
        autoRepeat(false).
        condition(new QnRunningVideowallActionCondition(manager));

    factory(QnActions::ClearVideoWallScreen).
        flags(Qn::Tree | Qn::VideoWallReviewScene | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Clear Screen")).
        autoRepeat(false).
        condition(new QnDetachFromVideoWallActionCondition(manager));

    factory(QnActions::SaveLayoutAction).
        flags(Qn::TitleBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredTargetPermissions(Qn::SavePermission).
        text(tr("Save Layout")).
        condition(new QnSaveLayoutActionCondition(false, manager));

    factory(QnActions::SaveLayoutAsAction).
        flags(Qn::SingleTarget | Qn::ResourceTarget).
        requiredTargetPermissions(Qn::UserResourceRole, Qn::SavePermission).    //TODO: #GDM #access check canCreateResource permission
        text(lit("If you see this string, notify me. #GDM")).
        condition(new QnSaveLayoutAsActionCondition(false, manager));

    factory(QnActions::SaveLayoutForCurrentUserAsAction).
        flags(Qn::TitleBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Save Layout As...")).
        condition(new QnSaveLayoutAsActionCondition(false, manager));

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(QnActions::DeleteVideoWallItemAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget | Qn::IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Delete")).
        condition(new QnForbiddenInSafeModeCondition(manager)).
        autoRepeat(false);

    factory(QnActions::MaximizeItemAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Maximize Item")).
        shortcut(lit("Enter")).
        shortcut(lit("Return")).
        autoRepeat(false).
        condition(new QnItemZoomedActionCondition(false, manager));

    factory(QnActions::UnmaximizeItemAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Restore Item")).
        shortcut(lit("Enter")).
        shortcut(lit("Return")).
        autoRepeat(false).
        condition(new QnItemZoomedActionCondition(true, manager));

    factory(QnActions::ShowInfoAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Show Info")).
        shortcut(lit("Alt+I")).
        condition(new QnDisplayInfoActionCondition(false, manager));

    factory(QnActions::HideInfoAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Hide Info")).
        shortcut(lit("Alt+I")).
        condition(new QnDisplayInfoActionCondition(true, manager));

    factory(QnActions::ToggleInfoAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::HotkeyOnly).
        text(tr("Toggle Info")).
        shortcut(lit("Alt+I")).
        condition(new QnDisplayInfoActionCondition(manager));

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Change Resolution...")).
        condition(new QnChangeResolutionActionCondition(manager));

    factory.beginSubMenu();
    {
        factory.beginGroup();
        factory(QnActions::RadassAutoAction).
            flags(Qn::Scene | Qn::NoTarget).
            text(tr("Auto")).
            checkable().
            checked();

        factory(QnActions::RadassLowAction).
            flags(Qn::Scene | Qn::NoTarget).
            text(tr("Low")).
            checkable();

        factory(QnActions::RadassHighAction).
            flags(Qn::Scene | Qn::NoTarget).
            text(tr("High")).
            checkable();
        factory.endGroup();
    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::SingleTarget).
        childFactory(new QnPtzPresetsToursActionFactory(manager)).
        text(tr("PTZ...")).
        requiredTargetPermissions(Qn::WritePtzPermission).
        condition(new QnPtzActionCondition(Qn::PresetsPtzCapability, false, manager));

    factory.beginSubMenu();
    {

        factory(QnActions::PtzSavePresetAction).
            mode(QnActionTypes::DesktopMode).
            flags(Qn::Scene | Qn::SingleTarget).
            text(tr("Save Current Position...")).
            requiredTargetPermissions(Qn::WritePtzPermission | Qn::SavePermission).
            condition(new QnPtzActionCondition(Qn::PresetsPtzCapability, true, manager));

        factory(QnActions::PtzManageAction).
            mode(QnActionTypes::DesktopMode).
            flags(Qn::Scene | Qn::SingleTarget).
            text(tr("Manage...")).
            requiredTargetPermissions(Qn::WritePtzPermission | Qn::SavePermission).
            condition(new QnPtzActionCondition(Qn::ToursPtzCapability, false, manager));

    } factory.endSubMenu();

    factory(QnActions::PtzCalibrateFisheyeAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Calibrate Fisheye")).
        condition(new QnPtzActionCondition(Qn::VirtualPtzCapability, false, manager));

#if 0
    factory(QnActions::ToggleRadassAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::HotkeyOnly).
        text(tr("Toggle Resolution Mode")).
        shortcut(lit("Alt+I")).
        condition(new QnDisplayInfoActionCondition(manager));
#endif

    factory(QnActions::StartSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Show Motion/Smart Search")).
        conditionalText(tr("Show Motion"), new QnNoArchiveActionCondition(manager)).
        shortcut(lit("Alt+G")).
        condition(new QnSmartSearchActionCondition(false, manager));

    // TODO: #ynikitenkov remove this action, use StartSmartSearchAction with checked state!
    factory(QnActions::StopSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Hide Motion/Smart Search")).
        conditionalText(tr("Hide Motion"), new QnNoArchiveActionCondition(manager)).
        shortcut(lit("Alt+G")).
        condition(new QnSmartSearchActionCondition(true, manager));

    factory(QnActions::ClearMotionSelectionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Clear Motion Selection")).
        condition(new QnClearMotionSelectionActionCondition(manager));

    factory(QnActions::ToggleSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::HotkeyOnly).
        text(tr("Toggle Smart Search")).
        shortcut(lit("Alt+G")).
        condition(new QnSmartSearchActionCondition(manager));

    factory(QnActions::CheckFileSignatureAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Check File Watermark")).
        shortcut(lit("Alt+C")).
        autoRepeat(false).
        condition(new QnCheckFileSignatureActionCondition(manager));

    factory(QnActions::TakeScreenshotAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::HotkeyOnly).
        text(tr("Take Screenshot")).
        shortcut(lit("Alt+S")).
        autoRepeat(false).
        condition(new QnTakeScreenshotActionCondition(manager));

    factory(QnActions::AdjustVideoAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Image Enhancement...")).
        shortcut(lit("Alt+J")).
        autoRepeat(false).
        condition(new QnAdjustVideoActionCondition(manager));

    factory(QnActions::CreateZoomWindowAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Create Zoom Window")).
        condition(new QnCreateZoomWindowActionCondition(manager));

    factory().
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Rotate to..."));

    factory.beginSubMenu();
    {
        factory(QnActions::Rotate0Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("0 degrees")).
            condition(new QnRotateItemCondition(manager));

        factory(QnActions::Rotate90Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("90 degrees")).
            condition(new QnRotateItemCondition(manager));

        factory(QnActions::Rotate180Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("180 degrees")).
            condition(new QnRotateItemCondition(manager));

        factory(QnActions::Rotate270Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("270 degrees")).
            condition(new QnRotateItemCondition(manager));
    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(QnActions::RemoveLayoutItemAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::LayoutItemTarget | Qn::IntentionallyAmbiguous).
        text(tr("Remove from Layout")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        autoRepeat(false).
        condition(new QnLayoutItemRemovalActionCondition(manager));

    factory(QnActions::RemoveLayoutItemFromSceneAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::LayoutItemTarget | Qn::IntentionallyAmbiguous).
        text(tr("Remove from Layout")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        autoRepeat(false).
        condition(new QnLayoutItemRemovalActionCondition(manager));

    factory(QnActions::RemoveFromServerAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::IntentionallyAmbiguous).
        requiredTargetPermissions(Qn::RemovePermission).
        text(tr("Delete")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        autoRepeat(false).
        condition(new QnResourceRemovalActionCondition(manager));

    factory(QnActions::StopSharingLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Stop Sharing Layout")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        autoRepeat(false).
        condition(new QnStopSharingActionCondition(manager));

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(QnActions::WebPageSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Edit...")).
        autoRepeat(false).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::web_page), Qn::ExactlyOne, manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
        );

    factory(QnActions::RenameResourceAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::IntentionallyAmbiguous).
        requiredTargetPermissions(Qn::WritePermission | Qn::WriteNamePermission).
        text(tr("Rename")).
        shortcut(lit("F2")).
        autoRepeat(false).
        condition(new QnRenameResourceActionCondition(manager));

    factory(QnActions::RenameVideowallEntityAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::VideoWallItemTarget | Qn::VideoWallMatrixTarget | Qn::IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Rename")).
        shortcut(lit("F2")).
        condition(new QnForbiddenInSafeModeCondition(manager)).
        autoRepeat(false);

    //TODO: #GDM #3.1 #tbd
    factory(QnActions::RenameLayoutTourAction).
        flags(Qn::Tree | Qn::NoTarget | Qn::IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Rename")).
        shortcut(lit("F2")).
        condition(
            QnActionConditionPtr(new QnTreeNodeTypeCondition(Qn::LayoutTourNode, manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
        ).
        autoRepeat(false);

    factory().
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        separator();

    //TODO: #gdm restore this functionality and allow to delete exported layouts
    factory(QnActions::DeleteFromDiskAction).
        //flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Delete from Disk")).
        autoRepeat(false).
        condition(hasFlags(Qn::local_media));

    factory(QnActions::SetAsBackgroundAction).
        flags(Qn::Scene | Qn::SingleTarget).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission).
        text(tr("Set as Layout Background")).
        autoRepeat(false).
        condition(
            QnActionConditionPtr(new QnSetAsBackgroundActionCondition(manager))
            && QnActionConditionPtr(new QnLightModeCondition(Qn::LightModeNoLayoutBackground, manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
        );

    factory(QnActions::UserSettingsAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("User Settings...")).
        requiredTargetPermissions(Qn::ReadPermission).
        condition(hasFlags(Qn::user));

    factory(QnActions::UserRolesAction).
        flags(Qn::Tree | Qn::NoTarget).
        text(tr("User Roles...")).
        conditionalText(tr("Role Settings..."), new QnTreeNodeTypeCondition(Qn::RoleNode, manager)).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            QnActionConditionPtr(new QnTreeNodeTypeCondition(Qn::UsersNode, manager))
            || QnActionConditionPtr(new QnTreeNodeTypeCondition(Qn::RoleNode, manager))
        );

    factory(QnActions::CameraIssuesAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        dynamicText(new QnDevicesNameActionTextFactory(
            QnCameraDeviceStringSet(
                tr("Check Device Issues..."), tr("Check Devices Issues..."),
                tr("Check Camera Issues..."), tr("Check Cameras Issues..."),
                tr("Check I/O Module Issues..."), tr("Check I/O Modules Issues...")
            ), manager)).
        requiredGlobalPermission(Qn::GlobalViewLogsPermission).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::Any, manager))
            && QnActionConditionPtr(new QnPreviewSearchModeCondition(true, manager))
        );

    factory(QnActions::CameraBusinessRulesAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        dynamicText(new QnDevicesNameActionTextFactory(
            QnCameraDeviceStringSet(
                tr("Device Rules..."), tr("Devices Rules..."),
                tr("Camera Rules..."), tr("Cameras Rules..."),
                tr("I/O Module Rules..."), tr("I/O Modules Rules...")
            ), manager)).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::ExactlyOne, manager))
            && QnActionConditionPtr(new QnPreviewSearchModeCondition(true, manager))
        );

    factory(QnActions::CameraSettingsAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        dynamicText(new QnDevicesNameActionTextFactory(
            QnCameraDeviceStringSet(
                tr("Device Settings..."), tr("Devices Settings..."),
                tr("Camera Settings..."), tr("Cameras Settings..."),
                tr("I/O Module Settings..."), tr("I/O Modules Settings...")
            ), manager)).
        requiredGlobalPermission(Qn::GlobalEditCamerasPermission).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::Any, manager))
            && QnActionConditionPtr(new QnPreviewSearchModeCondition(true, manager))
        );

    factory(QnActions::MediaFileSettingsAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("File Settings...")).
        condition(new QnResourceActionCondition(hasFlags(Qn::local_media), Qn::Any, manager));

    factory(QnActions::LayoutSettingsAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Layout Settings...")).
        requiredTargetPermissions(Qn::EditLayoutSettingsPermission).
        condition(new QnLightModeCondition(Qn::LightModeNoLayoutBackground, manager));

    factory(QnActions::VideowallSettingsAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Video Wall Settings...")).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::videowall), Qn::ExactlyOne, manager))
            && QnActionConditionPtr(new QnAutoStartAllowedActionCodition(manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
        );

    factory(QnActions::ServerAddCameraManuallyAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Add Device...")).   //intentionally hardcode devices here
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, manager))
            && QnActionConditionPtr(new QnEdgeServerCondition(false, manager))
            && ~QnActionConditionPtr(new QnFakeServerActionCondition(true, manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
        );

    factory(QnActions::CameraListByServerAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->resourcePool(),
            tr("Devices List by Server..."),
            tr("Cameras List by Server...")
        )).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, manager))
            && QnActionConditionPtr(new QnEdgeServerCondition(false, manager))
            && ~QnActionConditionPtr(new QnFakeServerActionCondition(true, manager))
        );

    factory(QnActions::PingAction).
        flags(Qn::NoTarget).
        text(tr("Ping..."));

    factory(QnActions::ServerLogsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Logs...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, manager))
            && ~QnActionConditionPtr(new QnFakeServerActionCondition(true, manager))
        );

    factory(QnActions::ServerIssuesAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Diagnostics...")).
        requiredGlobalPermission(Qn::GlobalViewLogsPermission).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, manager))
            && ~QnActionConditionPtr(new QnFakeServerActionCondition(true, manager))
        );

    factory(QnActions::WebAdminAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Web Page...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, manager))
            && ~QnActionConditionPtr(new QnFakeServerActionCondition(true, manager))
            && ~QnActionConditionPtr(new QnCloudServerActionCondition(Qn::Any, manager))
        );

    factory(QnActions::ServerSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Settings...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(
            QnActionConditionPtr(new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, manager))
            && ~QnActionConditionPtr(new QnFakeServerActionCondition(true, manager))
        );

    factory(QnActions::ConnectToCurrentSystem).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Merge to Currently Connected System...")).
        condition(
            QnActionConditionPtr(new QnTreeNodeTypeCondition(Qn::ResourceNode, manager))
            && QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            && QnActionConditionPtr(new QnMergeToCurrentSystemActionCondition(manager))
            && QnActionConditionPtr(new QnRequiresOwnerCondition(manager))
        );

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        childFactory(new QnAspectRatioActionFactory(manager)).
        text(tr("Change Cell Aspect Ratio...")).
        condition(
            ~QnActionConditionPtr(new QnVideoWallReviewModeCondition(manager))
            && QnActionConditionPtr(new QnLightModeCondition(Qn::LightModeSingleItem, manager))
        );

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Change Cell Spacing...")).
        condition(new QnLightModeCondition(Qn::LightModeSingleItem, manager));

    factory.beginSubMenu();
    {
        factory.beginGroup();

        factory(QnActions::SetCurrentLayoutItemSpacingNoneAction).
            flags(Qn::Scene | Qn::NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("None")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::None));

        factory(QnActions::SetCurrentLayoutItemSpacingSmallAction).
            flags(Qn::Scene | Qn::NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Small")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Small));

        factory(QnActions::SetCurrentLayoutItemSpacingMediumAction).
            flags(Qn::Scene | Qn::NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Medium")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Medium));

        factory(QnActions::SetCurrentLayoutItemSpacingLargeAction).
            flags(Qn::Scene | Qn::NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Large")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Large));
        factory.endGroup();

    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        separator();

    factory(QnActions::ReviewLayoutTourAction).
        flags(Qn::Tree | Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Review Layout Tour")).
        condition(new QnTreeNodeTypeCondition(Qn::LayoutTourNode, manager)).
        autoRepeat(false);

    factory(QnActions::ToggleLayoutTourModeAction).
        flags(Qn::Scene | Qn::Tree | Qn::NoTarget | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Start Tour")).
        shortcut(lit("Alt+T")).
        checkable().
        autoRepeat(false).
        condition(
            QnActionConditionPtr(new QnTreeNodeTypeCondition(Qn::LayoutTourNode, manager))
            && QnActionConditionPtr(new QnToggleTourActionCondition(manager))
        );

    factory(QnActions::RemoveLayoutTourAction).
        flags(Qn::Tree | Qn::NoTarget | Qn::IntentionallyAmbiguous).
        mode(QnActionTypes::DesktopMode).
        text(tr("Delete Layout Tour")).
        requiredGlobalPermission(Qn::GlobalAdminPermission). //TODO: #GDM #3.1 #tbd
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, Builder::Mac, true).
        condition(new QnTreeNodeTypeCondition(Qn::LayoutTourNode, manager));

    factory(QnActions::StartCurrentLayoutTourAction).
        flags(Qn::Scene | Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Start Tour")).
        accent(Qn::ButtonAccent::Standard).
        icon(qnSkin->icon("buttons/play.png")).
        condition(
            QnActionConditionPtr(new QnLayoutTourReviewModeCondition(manager))
            && QnActionConditionPtr(new QnStartCurrentLayoutTourActionCondition(manager))
        ).
        autoRepeat(false);

    factory(QnActions::SaveLayoutTourAction).
        flags(Qn::NoTarget).
        text(lit("Save layout tour (internal)")).
        mode(QnActionTypes::DesktopMode);

    factory(QnActions::SaveCurrentLayoutTourAction).
        flags(Qn::Scene | Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Save Changes")).
        condition(new QnLayoutTourReviewModeCondition(manager)).
        autoRepeat(false);

    factory(QnActions::RemoveCurrentLayoutTourAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        icon(qnSkin->icon("buttons/delete.png")).
        condition(new QnLayoutTourReviewModeCondition(manager)).
        autoRepeat(false);

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        separator();

    factory(QnActions::CurrentLayoutSettingsAction).
        flags(Qn::Scene | Qn::NoTarget).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission).
        text(tr("Layout Settings...")).
        condition(new QnLightModeCondition(Qn::LightModeNoLayoutBackground, manager));

    /* Tab bar actions. */
    factory().
        flags(Qn::TitleBar).
        separator();

    factory(QnActions::CloseLayoutAction).
        flags(Qn::TitleBar | Qn::ScopelessHotkey | Qn::SingleTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Close")).
        shortcut(lit("Ctrl+W")).
        autoRepeat(false);

    factory(QnActions::CloseAllButThisLayoutAction).
        flags(Qn::TitleBar | Qn::SingleTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Close All But This")).
        condition(new QnLayoutCountActionCondition(2, manager));

    /* Slider actions. */
    factory(QnActions::StartTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Mark Selection Start")).
        shortcut(lit("[")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new QnTimePeriodActionCondition(Qn::NullTimePeriod, Qn::InvisibleAction, manager));

    factory(QnActions::EndTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Mark Selection End")).
        shortcut(lit("]")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new QnTimePeriodActionCondition(Qn::EmptyTimePeriod, Qn::InvisibleAction, manager));

    factory(QnActions::ClearTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Clear Selection")).
        condition(new QnTimePeriodActionCondition(Qn::EmptyTimePeriod | Qn::NormalTimePeriod, Qn::InvisibleAction, manager));

    factory(QnActions::ZoomToTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Zoom to Selection")).
        condition(new QnTimePeriodActionCondition(Qn::NormalTimePeriod, Qn::InvisibleAction, manager));

    factory(QnActions::AddCameraBookmarkAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Add Bookmark...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(
            QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            && QnActionConditionPtr(new QnAddBookmarkActionCondition(manager))
        );

    factory(QnActions::EditCameraBookmarkAction).
        flags(Qn::Slider | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Edit Bookmark...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(
            QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            && QnActionConditionPtr(new QnModifyBookmarkActionCondition(manager))
        );

    factory(QnActions::RemoveCameraBookmarkAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Remove Bookmark...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(
            QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            && QnActionConditionPtr(new QnModifyBookmarkActionCondition(manager))
        );

    factory(QnActions::RemoveBookmarksAction).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Remove Bookmarks...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(
            QnActionConditionPtr(new QnForbiddenInSafeModeCondition(manager))
            && QnActionConditionPtr(new QnRemoveBookmarksActionCondition(manager))
        );

    factory().
        flags(Qn::Slider | Qn::SingleTarget).
        separator();

    factory(QnActions::ExportTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Export Selected Area...")).
        requiredTargetPermissions(Qn::ExportPermission).
        condition(new QnExportActionCondition(true, manager));

    factory(QnActions::ExportLayoutAction).
        flags(Qn::Slider | Qn::SingleTarget | Qn::MultiTarget | Qn::NoTarget).
        text(tr("Export Multi-Video...")).
        requiredTargetPermissions(Qn::CurrentLayoutMediaItemsRole, Qn::ExportPermission).
        condition(new QnExportActionCondition(false, manager));

    factory(QnActions::ExportTimelapseAction).
        flags(Qn::Slider | Qn::SingleTarget | Qn::MultiTarget | Qn::NoTarget).
        text(tr("Export Rapid Review...")).
        requiredTargetPermissions(Qn::CurrentLayoutMediaItemsRole, Qn::ExportPermission).
        condition(new QnExportActionCondition(true, manager));

    factory(QnActions::ThumbnailsSearchAction).
        flags(Qn::Slider | Qn::Scene | Qn::SingleTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Preview Search...")).
        condition(new QnPreviewActionCondition(manager));


    factory(QnActions::DebugIncrementCounterAction).
        flags(Qn::GlobalHotkey | Qn::DevMode).
        shortcut(lit("Ctrl+Alt+Shift++")).
        text(lit("Increment Debug Counter"));

    factory(QnActions::DebugDecrementCounterAction).
        flags(Qn::GlobalHotkey | Qn::DevMode).
        shortcut(lit("Ctrl+Alt+Shift+-")).
        text(lit("Decrement Debug Counter"));

    factory(QnActions::DebugCalibratePtzAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::DevMode).
        text(lit("Calibrate PTZ"));

    factory(QnActions::DebugGetPtzPositionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::DevMode).
        text(lit("Get PTZ Position"));

    factory(QnActions::DebugControlPanelAction).
        flags(Qn::GlobalHotkey | Qn::DevMode).
        shortcut(lit("Ctrl+Alt+Shift+D")).
        text(lit("Debug Control Panel"));

    factory(QnActions::PlayPauseAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Space")).
        text(tr("Play")).
        toggledText(tr("Pause")).
        condition(new QnArchiveActionCondition(manager));

    factory(QnActions::PreviousFrameAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Ctrl+Left")).
        text(tr("Previous Frame")).
        condition(new QnArchiveActionCondition(manager));

    factory(QnActions::NextFrameAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Ctrl+Right")).
        text(tr("Next Frame")).
        condition(new QnArchiveActionCondition(manager));

    factory(QnActions::JumpToStartAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Z")).
        text(tr("To Start")).
        condition(new QnArchiveActionCondition(manager));

    factory(QnActions::JumpToEndAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("X")).
        text(tr("To End")).
        condition(new QnArchiveActionCondition(manager));

    factory(QnActions::VolumeUpAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Ctrl+Up")).
        text(tr("Volume Down")).
        condition(new QnTimelineVisibleActionCondition(manager));

    factory(QnActions::VolumeDownAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Ctrl+Down")).
        text(tr("Volume Up")).
        condition(new QnTimelineVisibleActionCondition(manager));

    factory(QnActions::ToggleMuteAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("M")).
        text(tr("Toggle Mute")).
        checkable().
        condition(new QnTimelineVisibleActionCondition(manager));

    factory(QnActions::JumpToLiveAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("L")).
        text(tr("Jump to Live")).
        checkable().
        condition(new QnArchiveActionCondition(manager));

    factory(QnActions::ToggleSyncAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("S")).
        text(tr("Synchronize Streams")).
        toggledText(tr("Disable Stream Synchronization")).
        condition(new QnArchiveActionCondition(manager));


    factory().
        flags(Qn::Slider | Qn::TitleBar | Qn::Tree).
        separator();

    factory(QnActions::ToggleThumbnailsAction).
        flags(Qn::NoTarget).
        text(tr("Show Thumbnails")).
        toggledText(tr("Hide Thumbnails"));

    factory(QnActions::BookmarksModeAction).
        flags(Qn::NoTarget).
        text(tr("Show Bookmarks")).
        requiredGlobalPermission(Qn::GlobalViewBookmarksPermission).
        toggledText(tr("Hide Bookmarks"));

    factory(QnActions::ToggleCalendarAction).
        flags(Qn::NoTarget).
        text(tr("Show Calendar")).
        toggledText(tr("Hide Calendar"));

    factory(QnActions::ToggleTitleBarAction).
        flags(Qn::NoTarget).
        text(tr("Show Title Bar")).
        toggledText(tr("Hide Title Bar")).
        condition(new QnToggleTitleBarActionCondition(manager));

    factory(QnActions::PinTreeAction).
        flags(Qn::Tree | Qn::NoTarget).
        text(tr("Pin Tree")).
        toggledText(tr("Unpin Tree")).
        condition(new QnTreeNodeTypeCondition(Qn::RootNode, manager));

    factory(QnActions::PinCalendarAction).
        text(tr("Pin Calendar")).
        toggledText(tr("Unpin Calendar"));

    factory(QnActions::MinimizeDayTimeViewAction).
        text(tr("Minimize")).
        icon(qnSkin->icon("titlebar/dropdown.png"));

    factory(QnActions::ToggleTreeAction).
        flags(Qn::NoTarget).
        text(tr("Show Tree")).
        toggledText(tr("Hide Tree")).
        condition(new QnTreeNodeTypeCondition(Qn::RootNode, manager));

    factory(QnActions::ToggleTimelineAction).
        flags(Qn::NoTarget).
        text(tr("Show Timeline")).
        toggledText(tr("Hide Timeline"));

    factory(QnActions::ToggleNotificationsAction).
        flags(Qn::NoTarget).
        text(tr("Show Notifications")).
        toggledText(tr("Hide Notifications"));

    factory(QnActions::PinNotificationsAction).
        flags(Qn::Notifications | Qn::NoTarget).
        text(tr("Pin Notifications")).
        toggledText(tr("Unpin Notifications"));

    factory(QnActions::GoToNextItemAction)
        .flags(Qn::NoTarget);

    factory(QnActions::GoToPreviousItemAction)
        .flags(Qn::NoTarget);

    factory(QnActions::ToggleCurrentItemMaximizationStateAction)
        .flags(Qn::NoTarget);

    factory(QnActions::PtzContinuousMoveAction)
        .flags(Qn::NoTarget);

    factory(QnActions::PtzActivatePresetByIndexAction)
        .flags(Qn::NoTarget);
}
