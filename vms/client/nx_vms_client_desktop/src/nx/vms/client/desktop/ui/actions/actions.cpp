// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "actions.h"

#include <QtCore/QMetaEnum>

#include <client/client_runtime_settings.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/resource.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/layout_tour/layout_tour_actions.h>
#include <nx/vms/client/desktop/radass/radass_action_factory.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_conditions.h>
#include <nx/vms/client/desktop/ui/actions/action_factories.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_text_factories.h>
#include <nx/vms/client/desktop/ui/actions/factories/rotate_action_factory.h>
#include <nx/vms/client/desktop/ui/actions/menu_factory.h>
#include <nx/vms/client/desktop/workbench/timeline/timeline_actions_factory.h>
#include <ui/workbench/workbench_layout.h>

#include "actions.h"

namespace nx::vms::client::desktop::ui::action {

class ContextMenu
{
    Q_DECLARE_TR_FUNCTIONS(ContextMenu);
};

void initialize(Manager* manager, Action* root)
{
    MenuFactory factory(manager, root);

    /* Actions that are not assigned to any menu. */

    factory(ShowFpsAction)
        .flags(GlobalHotkey)
        .shortcut(lit("Ctrl+Alt+F"))
        .checkable()
        .autoRepeat(false);

    factory(DropResourcesAction)
        .flags(ResourceTarget | WidgetTarget | LayoutItemTarget | LayoutTarget | SingleTarget | MultiTarget)
        .mode(DesktopMode);

    factory(DelayedOpenVideoWallItemAction)
        .flags(NoTarget);

    factory(ProcessStartupParametersAction)
        .flags(NoTarget)
        .mode(DesktopMode | AcsMode | VideoWallMode);

    factory(MoveCameraAction)
        .flags(ResourceTarget | SingleTarget | MultiTarget)
        .requiredTargetPermissions(Qn::RemovePermission)
        .condition(condition::hasFlags(Qn::network, MatchMode::any));

    factory(NextLayoutAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .shortcut(lit("Ctrl+Tab"))
        .autoRepeat(false);

    factory(PreviousLayoutAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .shortcut(lit("Ctrl+Shift+Tab"))
        .autoRepeat(false);

    factory(SelectAllAction)
        .flags(GlobalHotkey)
        .shortcut(lit("Ctrl+A"))
        .shortcutContext(Qt::WidgetWithChildrenShortcut)
        .autoRepeat(false);

    factory(SelectionChangeAction)
        .flags(NoTarget);

    factory(SelectNewItemAction)
        .flags(NoTarget | SingleTarget | ResourceTarget);

    factory(PreferencesLicensesTabAction)
        .flags(NoTarget)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(PreferencesSmtpTabAction)
        .flags(NoTarget)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(PreferencesNotificationTabAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Filter...")); //< To be displayed on button tooltip

    factory(PreferencesCloudTabAction)
        .flags(NoTarget)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(ConnectAction)
        .flags(NoTarget);

    factory(SetupFactoryServerAction)
        .mode(DesktopMode)
        .flags(NoTarget);

    factory(ConnectToCloudSystemAction)
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("Connect to System"))
        .condition(condition::treeNodeType(ResourceTree::NodeType::cloudSystem));

    factory(ConnectToCloudSystemWithUserInteractionAction)
        .mode(DesktopMode)
        .flags(NoTarget)
        .condition(condition::isLoggedInToCloud()
            && condition::isCloudSystemConnectionUserInteractionRequired());

    factory(ReconnectAction)
        .flags(NoTarget);

    factory(FreespaceAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .shortcut(lit("F11"))
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(FullscreenResourceAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(ShowTimeLineOnVideowallAction)
        .flags(NoTarget)
        .mode(VideoWallMode)
        .condition(condition::isTrue(qnRuntime->videoWallWithTimeline()));

    factory(WhatsThisAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Help")) //< To be displayed on button tooltip
        .icon(qnSkin->icon("titlebar/window_question.png"));

    factory(CameraDiagnosticsAction)
        .mode(DesktopMode)
        .flags(ResourceTarget | SingleTarget)
        .condition(
            condition::hasFlags(
                /*require*/ Qn::live_cam,
                /*exclude*/ Qn::cross_system,
                MatchMode::any)
            && !condition::tourIsRunning());

    factory(OpenBusinessLogAction)
        .flags(NoTarget | SingleTarget | MultiTarget | ResourceTarget
            | LayoutItemTarget | WidgetTarget | GlobalHotkey)
        .mode(DesktopMode)
        .requiredGlobalPermission(GlobalPermission::viewLogs)
        .shortcut(lit("Ctrl+L"))
        .condition(!condition::tourIsRunning())
        .text(ContextMenu::tr("Event Log...")); //< To be displayed on button tooltip

    factory(OpenBusinessRulesAction)
        .mode(DesktopMode)
        .flags(NoTarget | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(OpenFailoverPriorityAction)
        .mode(DesktopMode)
        .flags(NoTarget)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(AcknowledgeEventAction)
        .mode(DesktopMode)
        .flags(NoTarget | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::manageBookmarks)
        .condition(condition::hasFlags(Qn::live_cam, MatchMode::all));

    factory(StartVideoWallControlAction)
        .flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Control Video Wall"))
        .condition(
            ConditionWrapper(new StartVideoWallControlCondition())
        );

    factory(PushMyScreenToVideowallAction)
        .flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Push my screen"))
        .condition(ConditionWrapper(new DesktopCameraCondition())
            && !condition::selectedItemsContainLockedLayout());

    factory(QueueAppRestartAction)
        .flags(NoTarget);

    factory(SelectTimeServerAction)
        .flags(NoTarget)
        .requiredGlobalPermission(GlobalPermission::admin)
        .text(ContextMenu::tr("Select Time Server")); //< To be displayed on button tooltip

    factory(PtzActivatePresetAction)
        .flags(SingleTarget | WidgetTarget)
        .requiredTargetPermissions(Qn::WritePtzPermission)
        .condition(new PtzCondition(Ptz::PresetsPtzCapability, false));

    factory(PtzActivateTourAction)
        .flags(SingleTarget | WidgetTarget)
        .requiredTargetPermissions(Qn::WritePtzPermission)
        .condition(new PtzCondition(Ptz::ToursPtzCapability, false));

    factory(PtzActivateObjectAction)
        .flags(SingleTarget | WidgetTarget)
        .requiredTargetPermissions(Qn::WritePtzPermission);

    factory(NotificationsTabAction)
        .flags(GlobalHotkey | HotkeyOnly)
        .shortcut(lit("N"))
        .condition(condition::isWorkbenchVisible())
        .text(ContextMenu::tr("Switch to Notifications tab"));

    factory(ResourcesTabAction)
        .flags(GlobalHotkey | HotkeyOnly)
        .shortcut("C")
        .condition(condition::isWorkbenchVisible())
        .text(ContextMenu::tr("Switch to Cameras & Resources tab"));

    factory(SearchResourcesAction)
        .flags(GlobalHotkey | HotkeyOnly)
        .shortcut("Ctrl+F")
        .condition(condition::isWorkbenchVisible())
        .text(ContextMenu::tr("Search Cameras & Resources"));

    factory(EditResourceInTreeAction)
        .flags(NoTarget)
        .condition(condition::isWorkbenchVisible());

    factory(MotionTabAction)
        .flags(GlobalHotkey | HotkeyOnly)
        .shortcut("M")
        .condition(condition::isWorkbenchVisible())
        .text(ContextMenu::tr("Switch to Motion tab"));

    factory(SwitchMotionTabAction)
        .flags(GlobalHotkey | HotkeyOnly)
        .shortcut("Alt+M");

    factory(BookmarksTabAction)
        .flags(GlobalHotkey | HotkeyOnly)
        .shortcut(lit("B"))
        .condition(condition::isWorkbenchVisible())
        .text(ContextMenu::tr("Switch to Bookmarks tab"));

    factory(EventsTabAction)
        .flags(GlobalHotkey | HotkeyOnly)
        .shortcut(lit("E"))
        .condition(condition::isWorkbenchVisible())
        .text(ContextMenu::tr("Switch to Events tab"));

    factory(ObjectsTabAction)
        .flags(GlobalHotkey | HotkeyOnly)
        .shortcut("O")
        .condition(condition::isWorkbenchVisible())
        .text(ContextMenu::tr("Switch to Objects tab"));

    factory(ObjectSearchModeAction)
        .flags(GlobalHotkey | HotkeyOnly)
        .shortcut("Alt+O")
        .checkable()
        .condition(condition::isWorkbenchVisible());

    factory(OpenAdvancedSearchDialog)
        .flags(NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Advanced..."));

    factory(SuspendCurrentTourAction)
        .flags(NoTarget)
        .condition(condition::tourIsRunning());

    factory(ResumeCurrentTourAction)
        .flags(NoTarget)
        .condition(condition::tourIsRunning());

    /* Context menu actions. */

    factory(FitInViewAction)
        .flags(Scene | NoTarget)
        .text(ContextMenu::tr("Fit in View"))
        .condition(!condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    factory()
        .flags(Scene)
        .separator();

    factory(MainMenuAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Main Menu")) //< To be displayed on button tooltip
        .shortcut(lit("Alt+Space"), Builder::Mac, true)
        .condition(!condition::tourIsRunning())
        .autoRepeat(false)
        .icon(qnSkin->icon("titlebar/main_menu.png"));

    factory(OpenLoginDialogAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Connect to Server..."))
        .conditionalText(ContextMenu::tr("Connect to Another Server..."),
            condition::isLoggedIn())
        .shortcut("Ctrl+Shift+C")
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(DisconnectAction)
        .flags(NoTarget)
        .condition(condition::isLoggedIn());

    factory(DisconnectMainMenuAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Disconnect from Server"))
        .autoRepeat(false)
        .shortcut("Ctrl+Shift+D")
        .condition(condition::isLoggedIn());

    factory(ResourcesModeAction)
        .flags(Main)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Browse Local Files"))
        .toggledText(ContextMenu::tr("Show Welcome Screen"))
        .condition(!condition::isLoggedIn());

    factory()
        .flags(Main | Tree)
        .separator();

    factory()
        .flags(Main | TitleBar | Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("New"));

    factory.beginSubMenu();
    {
        factory(OpenNewTabAction)
            .flags(Main | TitleBar | SingleTarget | NoTarget | GlobalHotkey)
            .mode(DesktopMode)
            .text(ContextMenu::tr("Tab"))
            .pulledText(ContextMenu::tr("New Tab"))
            .shortcut(lit("Ctrl+T"))
            .condition(!condition::tourIsRunning())
            .autoRepeat(false) /* Technically, it should be auto-repeatable, but we don't want the user opening 100500 layouts and crashing the client =). */
            .icon(qnSkin->icon("titlebar/new_layout.png"));

        factory(OpenNewWindowAction)
            .flags(Main | GlobalHotkey)
            .mode(DesktopMode)
            .text(ContextMenu::tr("Window"))
            .pulledText(ContextMenu::tr("New Window"))
            .shortcut(lit("Ctrl+N"))
            .autoRepeat(false)
            .condition(condition::isLoggedIn()
                 && ConditionWrapper(new LightModeCondition(Qn::LightModeNoNewWindow)));

        factory(OpenWelcomeScreenAction)
            .flags(Main | GlobalHotkey)
            .mode(DesktopMode)
            .text(ContextMenu::tr("Welcome Screen"))
            .pulledText(ContextMenu::tr("Welcome Screen"))
            .autoRepeat(false)
            .condition(new LightModeCondition(Qn::LightModeNoNewWindow));

        factory()
            .flags(Main)
            .separator();

        factory(NewUserAction)
            .flags(Main | Tree)
            .requiredGlobalPermission(GlobalPermission::admin)
            .text(ContextMenu::tr("User..."))
            .pulledText(ContextMenu::tr("New User..."))
            .condition(
                condition::treeNodeType(ResourceTree::NodeType::users)
            )
            .autoRepeat(false);

        factory(NewVideoWallAction)
            .flags(Main)
            .requiredGlobalPermission(GlobalPermission::admin)
            .text(ContextMenu::tr("Video Wall..."))
            .pulledText(ContextMenu::tr("New Video Wall..."))
            .autoRepeat(false);

        factory(NewWebPageAction)
            .flags(Main | Tree)
            .requiredGlobalPermission(GlobalPermission::admin)
            .text(ContextMenu::tr("Web Page..."))
            .pulledText(ContextMenu::tr("Add Web Page..."))
            .condition(
                condition::treeNodeType(ResourceTree::NodeType::webPages)
            )
            .autoRepeat(false);

        factory(NewLayoutTourAction)
            .flags(Main | Tree | NoTarget)
            .text(ContextMenu::tr("Showreel..."))
            .pulledText(ContextMenu::tr("New Showreel..."))
            .condition(condition::isLoggedIn()
                && condition::treeNodeType(ResourceTree::NodeType::layoutTours)
            )
            .autoRepeat(false);

        factory(NewVirtualCameraAction)
            .flags(Main | NoTarget)
            .requiredGlobalPermission(GlobalPermission::admin)
            .text(ContextMenu::tr("Virtual Camera..."))
            .pulledText(ContextMenu::tr("New Virtual Camera..."))
            .condition(condition::isLoggedIn())
            .autoRepeat(false);
    }
    factory.endSubMenu();

    factory(NewUserLayoutAction)
        .flags(Tree | SingleTarget | ResourceTarget | NoTarget)
        .text(ContextMenu::tr("New Layout..."))
        .condition(
            ConditionWrapper(new NewUserLayoutCondition())
        );

    factory(OpenCurrentUserLayoutMenu)
        .flags(TitleBar | SingleTarget | NoTarget)
        .text(ContextMenu::tr("Open Layout...")) //< To be displayed on button tooltip
        .childFactory(new OpenCurrentUserLayoutFactory(manager))
        .icon(qnSkin->icon("titlebar/dropdown.png"));

    factory()
        .flags(TitleBar)
        .separator();

    factory(ShowServersInTreeAction)
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("Show Servers"))
        .autoRepeat(false)
        .checkable()
        .checked(false) //< This action will be kept in unchecked state.
        .condition(condition::isLoggedIn()
            && condition::treeNodeType({ResourceTree::NodeType::camerasAndDevices})
            && condition::allowedToShowServersInResourceTree());

    factory(HideServersInTreeAction)
        .flags(Tree | NoTarget | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Show Servers"))
        .autoRepeat(false)
        .checkable()
        .checked(true) //< This action will be kept in checked state.
        .condition(new HideServersInTreeCondition());

    factory().separator()
        .flags(Tree);

    factory()
        .flags(Main | Tree | Scene)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open"))
        .condition(!condition::tourIsRunning());

    factory.beginSubMenu();
    {
        factory(OpenFileAction)
            .flags(Main | Scene | NoTarget | GlobalHotkey)
            .mode(DesktopMode)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
            .text(ContextMenu::tr("Files..."))
            .shortcut(lit("Ctrl+O"))
            .condition(!condition::tourIsRunning())
            .autoRepeat(false);

        factory(OpenFolderAction)
            .flags(Main | Scene)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
            .condition(!condition::tourIsRunning())
            .text(ContextMenu::tr("Folder..."));

        factory().separator()
            .flags(Main);

        factory(WebClientAction)
            .flags(Main | Tree | NoTarget)
            .text(ContextMenu::tr("Web Client..."))
            .pulledText(ContextMenu::tr("Open Web Client..."))
            .autoRepeat(false)
            .condition(condition::isLoggedIn()
                && condition::treeNodeType({ResourceTree::NodeType::currentSystem,
                    ResourceTree::NodeType::servers, ResourceTree::NodeType::camerasAndDevices}));

    } factory.endSubMenu();

    factory(SaveCurrentLayoutTourAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .condition(condition::isLayoutTourReviewMode())
        .autoRepeat(false);

    factory(SaveCurrentLayoutAction)
        .mode(DesktopMode)
        .flags(Scene | NoTarget | GlobalHotkey | IntentionallyAmbiguous)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::SavePermission)
        .text(ContextMenu::tr("Save Current Layout"))
        .shortcut(lit("Ctrl+S"))
        .autoRepeat(false) /* There is no point in saving the same layout many times in a row. */
        .condition(ConditionWrapper(new SaveLayoutCondition(true))
            && !condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    factory(SaveCurrentLayoutAsAction)
        .mode(DesktopMode)
        .flags(Scene | NoTarget | GlobalHotkey)
        .text(ContextMenu::tr("Save Current Layout As..."))
        .shortcut("Ctrl+Shift+S")
        .shortcut(QKeySequence("Ctrl+Alt+S"), Builder::Windows, false)
        .autoRepeat(false)
        .condition(
            condition::isLoggedIn()
            && condition::applyToCurrentLayout(condition::canSaveLayoutAs())
            && !condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    factory(SaveCurrentLayoutAsCloudAction)
        .mode(DesktopMode)
        .flags(Scene | NoTarget)
        .text(ContextMenu::tr("Save Current Layout As Cloud..."))
        .autoRepeat(false)
        .condition(
            condition::isLoggedIn()
            && condition::isLoggedInToCloud()
            && condition::applyToCurrentLayout(
                condition::canSaveLayoutAs()
                && condition::hasFlags(Qn::cross_system, MatchMode::none))
            && !condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    factory(ShareLayoutAction)
        .mode(DesktopMode)
        .flags(SingleTarget | ResourceTarget)
        .autoRepeat(false)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(ShareCameraAction)
        .mode(DesktopMode)
        .flags(SingleTarget | ResourceTarget)
        .autoRepeat(false)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(ShareWebPageAction)
        .mode(DesktopMode)
        .flags(SingleTarget | ResourceTarget)
        .autoRepeat(false)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(SaveCurrentVideoWallReviewAction)
        .flags(Main | Scene | NoTarget | GlobalHotkey | IntentionallyAmbiguous)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Save Video Wall View"))
        .shortcut(lit("Ctrl+S"))
        .autoRepeat(false)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .condition(
            ConditionWrapper(new SaveVideowallReviewCondition(true))
        );

    factory(DropOnVideoWallItemAction)
        .flags(ResourceTarget | LayoutItemTarget | LayoutTarget | VideoWallItemTarget | SingleTarget | MultiTarget)
        .requiredGlobalPermission(GlobalPermission::controlVideowall);

    factory()
        .flags(Main)
        .separator();

    const bool screenRecordingSupported = nx::build_info::isWindows();
    if (screenRecordingSupported && qnRuntime->isDesktopMode())
    {
        factory(ToggleScreenRecordingAction)
            .flags(Main | GlobalHotkey)
            .mode(DesktopMode)
            .text(ContextMenu::tr("Start Screen Recording"))
            .toggledText(ContextMenu::tr("Stop Screen Recording"))
            .shortcut(lit("Alt+R"))
            .shortcut(Qt::Key_MediaRecord)
            .shortcutContext(Qt::ApplicationShortcut)
            .autoRepeat(false)
            .condition(!condition::isLoggedIn()
                || condition::hasGlobalPermission(GlobalPermission::exportArchive));

        factory()
            .flags(Main)
            .separator();
    }

    factory(EscapeHotkeyAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .autoRepeat(false)
        .shortcut(lit("Esc"))
        .text(ContextMenu::tr("Stop current action"));

    factory(FullscreenAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Go to Fullscreen"))
        .toggledText(ContextMenu::tr("Exit Fullscreen"))
        .shortcut(QKeySequence::FullScreen, Builder::Mac, true)
        .shortcutContext(Qt::ApplicationShortcut)
        .icon(qnSkin->icon("titlebar/window_maximize.png", "titlebar/window_restore.png"));


    factory(MinimizeAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Minimize"))
        .icon(qnSkin->icon("titlebar/window_minimize.png"));

    factory(MaximizeAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Maximize"))
        .toggledText(ContextMenu::tr("Restore Down"))
        .autoRepeat(false)
        .icon(qnSkin->icon("titlebar/window_maximize.png", "titlebar/window_restore.png"));


    factory(FullscreenMaximizeHotkeyAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .autoRepeat(false)
        .shortcut(lit("Alt+Enter"))
        .shortcut(lit("Alt+Return"))
        .shortcutContext(Qt::ApplicationShortcut)
        .condition(PreventWhenFullscreenTransition::condition());

    factory(VersionMismatchMessageAction)
        .flags(NoTarget)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(BetaVersionMessageAction)
        .flags(NoTarget)
        .mode(DesktopMode);

    factory(BetaUpgradeWarningAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .condition(condition::showBetaUpgradeWarning());

    factory(ConfirmAnalyticsStorageAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(BrowseUrlAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open in Browser..."));

    factory(SystemAdministrationAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("System Administration..."))
        .shortcut(lit("Ctrl+Alt+A"))
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(
            condition::treeNodeType({ResourceTree::NodeType::currentSystem,
                ResourceTree::NodeType::servers, ResourceTree::NodeType::camerasAndDevices})
            && !condition::tourIsRunning());

    factory(SystemUpdateAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("System Update..."))
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(AdvancedUpdateSettingsAction)
        .flags(NoTarget)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(UserManagementAction)
        .flags(Main | Tree)
        .requiredGlobalPermission(GlobalPermission::admin)
        .text(ContextMenu::tr("User Management..."))
        .condition(condition::treeNodeType(ResourceTree::NodeType::users));

    factory(LogsManagementAction)
        .flags(NoTarget)
        .requiredGlobalPermission(GlobalPermission::admin);

    factory(PreferencesGeneralTabAction)
        .flags(Main)
        .text(ContextMenu::tr("Local Settings..."))
        //.shortcut(lit("Ctrl+P"))
        .role(QAction::PreferencesRole)
        .autoRepeat(false);

    factory(JoystickSettingsAction)
        .flags(Main)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Joystick Settings..."))
        .autoRepeat(false)
        .condition(condition::joystickConnected());

    factory(OpenAuditLogAction)
        .flags(Main)
        .requiredGlobalPermission(GlobalPermission::admin)
        .text(ContextMenu::tr("Audit Trail..."));

    factory(OpenBookmarksSearchAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .requiredGlobalPermission(GlobalPermission::viewBookmarks)
        .text(ContextMenu::tr("Bookmark Log..."))
        .shortcut(lit("Ctrl+B"))
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(LoginToCloud)
        .flags(NoTarget)
        .text(ContextMenu::tr("Log in to %1...", "Log in to Nx Cloud")
            .arg(nx::branding::cloudName()));

    factory(LogoutFromCloud)
        .flags(NoTarget)
        .text(ContextMenu::tr("Log out from %1", "Log out from Nx Cloud")
            .arg(nx::branding::cloudName()));

    factory(OpenCloudAccountSecurityUrl)
        .flags(NoTarget)
        .text(ContextMenu::tr("Account Security..."));

    factory(OpenCloudMainUrl)
        .flags(NoTarget)
        .text(ContextMenu::tr("Open %1 Portal...", "Open Nx Cloud Portal")
            .arg(nx::branding::cloudName()));

    factory(OpenCloudViewSystemUrl)
        .flags(NoTarget);

    factory(OpenCloudManagementUrl)
        .flags(NoTarget)
        .text(ContextMenu::tr("Account Settings..."));

    factory(HideCloudPromoAction)
        .flags(NoTarget);

    factory(ChangeDefaultCameraPasswordAction)
        .flags(SingleTarget | MultiTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::admin)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Some cameras require passwords to be set"));

    factory(OpenCloudRegisterUrl)
        .flags(NoTarget)
        .text(ContextMenu::tr("Create Account..."));

    factory()
        .flags(Main)
        .separator();

    factory(BusinessEventsAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .requiredGlobalPermission(GlobalPermission::admin)
        .text(ContextMenu::tr("Event Rules..."))
        .shortcut(lit("Ctrl+E"))
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(CameraListAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .requiredGlobalPermission(GlobalPermission::admin)
        .text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->resourcePool(),
            ContextMenu::tr("Devices List"),
            ContextMenu::tr("Cameras List")
        ))
        .shortcut(lit("Ctrl+M"))
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(MainMenuAddDeviceManuallyAction)
        .flags(Main)
        .text(ContextMenu::tr("Add Device..."))
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(condition::isLoggedIn()
            && !condition::tourIsRunning());

    factory(MergeSystems)
        .flags(Main | Tree)
        .text(ContextMenu::tr("Merge Systems..."))
        .condition(
            condition::treeNodeType({ResourceTree::NodeType::currentSystem,
                ResourceTree::NodeType::servers, ResourceTree::NodeType::camerasAndDevices})
            && ConditionWrapper(new RequiresOwnerCondition())
        );

    // TODO: Implement proxy actions to allow the same action to be shown in different locations.
    const auto systemAdministrationAction = manager->action(SystemAdministrationAction);
    const auto systemAdministrationAlias = factory.registerAction()
        .flags(Tree | NoTarget)
        .mode(DesktopMode)
        .text(systemAdministrationAction->text())
        .shortcut("Ctrl+Alt+A")
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(
            condition::treeNodeType({ResourceTree::NodeType::currentSystem,
                ResourceTree::NodeType::servers, ResourceTree::NodeType::camerasAndDevices})
            && !condition::tourIsRunning()).action();

    QObject::connect(systemAdministrationAlias, &QAction::triggered,
        systemAdministrationAction, &QAction::trigger);

    factory()
        .flags(Main)
        .separator();

    factory(AboutAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("About..."))
        .shortcut(lit("F1"))
        .shortcutContext(Qt::ApplicationShortcut)
        .role(QAction::AboutRole)
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(UserManualAction)
        .flags(Main)
        .mode(DesktopMode)
        .text(ContextMenu::tr("User Manual..."))
        .condition(!condition::tourIsRunning());

    factory()
        .flags(Main)
        .separator();

    factory(SaveSessionState)
        .flags(Main)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Save Window Configuration"))
        .conditionalText(ContextMenu::tr("Save Windows Configuration"),
            condition::hasOtherWindowsInSession())
        .condition(condition::isLoggedIn() && !condition::hasSavedWindowsState())
        .autoRepeat(false);

    factory()
        .flags(Main)
        .mode(DesktopMode)
        //.childFactory(???)
        .text(ContextMenu::tr("Windows Configuration"))
        .condition(condition::isLoggedIn() && condition::hasSavedWindowsState());

    factory.beginSubMenu();
    {
        const auto saveWindowStateAction = manager->action(SaveSessionState);
        const auto saveWindowStateActionAlias = factory.registerAction()
            .flags(Main)
            .mode(DesktopMode)
            .text(ContextMenu::tr("Save Current State"))
            .action();
        QObject::connect(saveWindowStateActionAlias, &QAction::triggered,
            saveWindowStateAction, &QAction::trigger);

        factory()
            .flags(Main)
            .separator();

        factory(RestoreSessionState)
            .flags(Main)
            .mode(DesktopMode)
            .text(ContextMenu::tr("Restore Saved State"));

        factory(DeleteSessionState)
            .flags(Main)
            .mode(DesktopMode)
            .text(ContextMenu::tr("Delete Saved State"));

    }
    factory.endSubMenu();

    factory()
        .flags(Main)
        .separator();

    factory(CloseAllWindowsAction)
        .flags(Main)
        .text(ContextMenu::tr("Close all Windows"))
        .condition(condition::hasOtherWindows());

    factory(ExitAction)
        .flags(Main | GlobalHotkey)
        .text(ContextMenu::tr("Exit"))
        .shortcut(lit("Alt+F4"))
        .shortcut(lit("Ctrl+Q"), Builder::Mac, true)
        .shortcutContext(Qt::ApplicationShortcut)
        .role(QAction::QuitRole)
        .autoRepeat(false)
        .icon(qnSkin->icon("titlebar/window_close.png"))
        .iconVisibleInMenu(false);

    factory(DelayedForcedExitAction)
        .flags(NoTarget);

    factory(BeforeExitAction)
        .flags(NoTarget);

    factory(RemoveSystemFromTabBarAction)
        .flags(NoTarget);

//-------------------------------------------------------------------------------------------------
// Slider actions.


//-------------------------------------------------------------------------------------------------
// Selection actions section.

    factory(ChunksFilterAction)
        .flags(Slider | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Archive filter..."))
        .childFactory(new ChunksFilterActionFactory(manager))
        .requiredGlobalPermission(GlobalPermission::viewArchive);

    factory()
        .flags(Slider)
        .separator();

    factory(StartTimeSelectionAction)
        .flags(Slider | SingleTarget)
        .text(ContextMenu::tr("Mark Selection Start"))
        .shortcut(lit("["))
        .shortcutContext(Qt::WidgetShortcut)
        .condition(new TimePeriodCondition(NullTimePeriod, InvisibleAction));

    factory(EndTimeSelectionAction)
        .flags(Slider | SingleTarget)
        .text(ContextMenu::tr("Mark Selection End"))
        .shortcut(lit("]"))
        .shortcutContext(Qt::WidgetShortcut)
        .condition(new TimePeriodCondition(EmptyTimePeriod, InvisibleAction));

    factory(ClearTimeSelectionAction)
        .flags(Slider | SingleTarget)
        .text(ContextMenu::tr("Clear Selection"))
        .condition(new TimePeriodCondition(EmptyTimePeriod | NormalTimePeriod, InvisibleAction));

    factory(ZoomToTimeSelectionAction)
        .flags(Slider | SingleTarget)
        .text(ContextMenu::tr("Zoom to Selection"))
        .condition(new TimePeriodCondition(NormalTimePeriod, InvisibleAction));

    factory()
        .flags(Slider)
        .separator();

//-------------------------------------------------------------------------------------------------
// Bookmarks actions section.

    factory(AddCameraBookmarkAction)
        .flags(Slider | SingleTarget)
        .text(ContextMenu::tr("Add Bookmark..."))
        .requiredGlobalPermission(GlobalPermission::manageBookmarks)
        .condition(
            ConditionWrapper(new AddBookmarkCondition())
        );

    factory()
        .flags(Slider)
        .separator();

    factory(EditCameraBookmarkAction)
        .flags(Slider | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Edit Bookmark..."))
        .requiredGlobalPermission(GlobalPermission::manageBookmarks)
        .condition(
            ConditionWrapper(new ModifyBookmarkCondition())
        );

    factory(RemoveBookmarkAction)
        .flags(Slider | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Delete Bookmark..."))
        .requiredGlobalPermission(GlobalPermission::manageBookmarks)
        .condition(
            ConditionWrapper(new ModifyBookmarkCondition())
        );

    factory(RemoveBookmarksAction)
        .flags(NoTarget | ResourceTarget)
        .text(ContextMenu::tr("Delete Bookmarks...")) //< Copied to an internal context menu
        .requiredGlobalPermission(GlobalPermission::manageBookmarks)
        .condition(
            ConditionWrapper(new RemoveBookmarksCondition())
        );

    factory(ExportBookmarkAction)
        .flags(Slider | SingleTarget | MultiTarget | NoTarget | WidgetTarget | ResourceTarget)
        .text(ContextMenu::tr("Export Bookmark..."))
        .condition(condition::canExportBookmark());

    factory(ExportBookmarksAction)
        .flags(NoTarget | MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Export Bookmarks..."))
        .condition(condition::canExportBookmarks());

    factory(CopyBookmarkTextAction)
        .flags(Slider | SingleTarget | WidgetTarget | ResourceTarget)
        .text(ContextMenu::tr("Copy Bookmark Text"))
        .condition(condition::canExportBookmark());

    factory(CopyBookmarksTextAction)
        .flags(NoTarget | ResourceTarget)
        .text(ContextMenu::tr("Copy Bookmarks Text"))
        .condition(condition::canExportBookmarks());

    factory()
        .flags(Slider)
        .separator();

//-------------------------------------------------------------------------------------------------
// Selected time period actions section.

    factory(ExportVideoAction)
        .flags(Slider | SingleTarget | MultiTarget | NoTarget | WidgetTarget | ResourceTarget)
        .text(ContextMenu::tr("Export Video..."))
        .condition(condition::canExportLayout());

    factory(ThumbnailsSearchAction)
        .flags(Slider | Scene | SingleTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Preview Search..."))
        .condition(new PreviewCondition());

//-------------------------------------------------------------------------------------------------

    factory()
        .flags(Tree | SingleTarget | ResourceTarget)
        .childFactory(new EdgeNodeFactory(manager))
        .text(ContextMenu::tr("Server..."))
        .condition(condition::treeNodeType(ResourceTree::NodeType::edge));

    factory()
        .flags(Scene | Tree)
        .separator();

    /* Resource actions. */
    factory(OpenInLayoutAction)
        .flags(SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .requiredTargetPermissions(Qn::LayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
        .condition(new OpenInLayoutCondition());

    factory(OpenInCurrentLayoutAction)
        .flags(Tree | Table | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
        .text(ContextMenu::tr("Open"))
        .conditionalText(ContextMenu::tr("Monitor"),
            condition::hasFlags(Qn::server, MatchMode::all))
        .condition(
            ConditionWrapper(new OpenInCurrentLayoutCondition())
            && !condition::isLayoutTourReviewMode());

    factory(OpenInNewTabAction)
        .mode(DesktopMode)
        .flags(Tree | Table | Scene | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .text(ContextMenu::tr("Open in New Tab"))
        .conditionalText(ContextMenu::tr("Monitor in New Tab"),
            condition::hasFlags(Qn::server, MatchMode::all))
        .condition(new OpenInNewEntityCondition());

    factory(OpenIntercomLayoutAction)
        .mode(DesktopMode)
        .flags(SingleTarget | ResourceTarget)
        .condition(new OpenInNewEntityCondition());

    factory(OpenInAlarmLayoutAction)
        .mode(DesktopMode)
        .flags(SingleTarget | MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Open in Alarm Layout"));

    factory(OpenInNewWindowAction)
        .mode(DesktopMode)
        .flags(Tree | Table | Scene | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .text(ContextMenu::tr("Open in New Window"))
        .conditionalText(ContextMenu::tr("Monitor in New Window"),
            condition::hasFlags(Qn::server, MatchMode::all))
        .condition(
            ConditionWrapper(new OpenInNewEntityCondition())
            && ConditionWrapper(new LightModeCondition(Qn::LightModeNoNewWindow))
        );

    factory(OpenCurrentLayoutInNewWindowAction)
        .flags(NoTarget)
        .condition(new LightModeCondition(Qn::LightModeNoNewWindow));

    factory(OpenVideoWallReviewAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Open Video Wall"))
        .condition(condition::hasFlags(Qn::videowall, MatchMode::any));

    factory(OpenInFolderAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Open Containing Folder"))
        .autoRepeat(false)
        .condition(new OpenInFolderCondition());

    factory(IdentifyVideoWallAction)
        .flags(Tree | Scene | SingleTarget | MultiTarget | ResourceTarget | VideoWallItemTarget)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Identify"))
        .autoRepeat(false)
        .condition(new IdentifyVideoWallCondition());

    factory(AttachToVideoWallAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::admin)
        .text(ContextMenu::tr("Attach to Video Wall..."))
        .autoRepeat(false)
        .condition(
            condition::hasFlags(Qn::videowall, MatchMode::any)
        );

    factory(StartVideoWallAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Switch to Video Wall mode..."))
        .autoRepeat(false)
        .condition(new StartVideowallCondition());

    factory(SaveVideoWallReviewAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Save Video Wall"))
        .shortcut(lit("Ctrl+S"))
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .autoRepeat(false)
        .condition(
            ConditionWrapper(new SaveVideowallReviewCondition(false))
        );

    factory(SaveVideowallMatrixAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Save Current Matrix"))
        .autoRepeat(false)
        .condition(
            ConditionWrapper(new NonEmptyVideowallCondition())
        );

    factory(LoadVideowallMatrixAction)
        .flags(Tree | SingleTarget | VideoWallMatrixTarget)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Load Matrix"));

    factory(DeleteVideowallMatrixAction)
        .flags(Tree | SingleTarget | MultiTarget | VideoWallMatrixTarget | IntentionallyAmbiguous)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Delete"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .autoRepeat(false);

    factory()
        .flags(Scene | Tree)
        .separator();

    factory(StopVideoWallAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Stop Video Wall"))
        .autoRepeat(false)
        .condition(new RunningVideowallCondition());

    factory(ClearVideoWallScreen)
        .flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Clear Screen"))
        .autoRepeat(false)
        .condition(ConditionWrapper(new DetachFromVideoWallCondition())
            && !condition::selectedItemsContainLockedLayout());

    factory(SelectCurrentServerAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Connect to this Server"))
        .condition(new ReachableServerCondition());

    factory()
        .flags(Tree | VideoWallReviewScene | SingleTarget | VideoWallItemTarget)
        .separator();

    factory(VideoWallScreenSettingsAction)
        .flags(Tree | VideoWallReviewScene | SingleTarget | VideoWallItemTarget)
        .requiredGlobalPermission(nx::vms::api::GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Screen Settings..."));

    factory(ForgetLayoutPasswordAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Forget password"))
        .condition(condition::canForgetPassword() && !condition::isLayoutTourReviewMode());

    factory(SaveLayoutAction)
        .flags(TitleBar | Tree | SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::SavePermission)
        .dynamicText(new FunctionalTextFactory(
            [](const Parameters& parameters, QnWorkbenchContext* /*context*/)
            {
                return parameters.resource()->hasFlags(Qn::cross_system)
                    ? ContextMenu::tr("Save Cloud Layout")
                    : ContextMenu::tr("Save Layout");
            },
            manager))
        .condition(ConditionWrapper(new SaveLayoutCondition(false)));

    factory(SaveLocalLayoutAction)
        .flags(SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::SavePermission)
        .condition(condition::hasFlags(Qn::layout, MatchMode::all));

    factory(SaveLayoutAsAction)
        .flags(TitleBar | Tree | SingleTarget | ResourceTarget)
        .dynamicText(new FunctionalTextFactory(
            [](const Parameters& parameters, QnWorkbenchContext* /*context*/)
            {
                return parameters.resource()->hasFlags(Qn::cross_system)
                    ? ContextMenu::tr("Save Cloud Layout As...")
                    : ContextMenu::tr("Save Layout As...");
            },
            manager))
        .condition(
            condition::isLoggedIn()
            && condition::canSaveLayoutAs()
            && !condition::isLayoutTourReviewMode()
        );

    factory(SaveLayoutAsCloudAction)
        .flags(TitleBar | Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Save Layout As Cloud..."))
        .condition(
            condition::canSaveLayoutAs()
            && condition::isLoggedInToCloud()
            && condition::hasFlags(Qn::cross_system, MatchMode::none)
        );

    factory()
        .flags(Tree)
        .separator();

    factory(MakeLayoutTourAction)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Make Showreel"))
        .condition(condition::canMakeShowreel());

    factory()
        .flags(Scene | Tree)
        .separator();

    factory(DeleteVideoWallItemAction)
        .flags(Tree | SingleTarget | MultiTarget | VideoWallItemTarget | IntentionallyAmbiguous)
        .requiredGlobalPermission(GlobalPermission::admin)
        .text(ContextMenu::tr("Delete"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .autoRepeat(false);

    factory(MaximizeItemAction)
        .flags(Scene | SingleTarget)
        .text(ContextMenu::tr("Maximize Item"))
        .shortcut(lit("Enter"))
        .shortcut(lit("Return"))
        .autoRepeat(false)
        .condition(ConditionWrapper(new ItemZoomedCondition(false))
            && !condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    factory(UnmaximizeItemAction)
        .flags(Scene | SingleTarget)
        .text(ContextMenu::tr("Restore Item"))
        .shortcut(lit("Enter"))
        .shortcut(lit("Return"))
        .autoRepeat(false)
        .condition(ConditionWrapper(new ItemZoomedCondition(true))
            && !condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    factory()
        .flags(Scene | SingleTarget | MultiTarget)
        .childFactory(new ShowOnItemsFactory(manager))
        .dynamicText(new FunctionalTextFactory(
            [](const Parameters& parameters, QnWorkbenchContext* /*context*/)
            {
                return ContextMenu::tr("Show on Items", "", parameters.resources().size());
            },
            manager));

    // Used via the hotkey.
    factory(ToggleInfoAction)
        .flags(Scene | SingleTarget | MultiTarget | HotkeyOnly)
        .shortcut("I")
        .shortcut("Alt+I")
        .condition(ConditionWrapper(new DisplayInfoCondition())
            && !condition::isLayoutTourReviewMode());

    factory()
        .flags(Scene | SingleTarget)
        .childFactory(new PtzPresetsToursFactory(manager))
        .text(ContextMenu::tr("PTZ"))
        .requiredTargetPermissions(Qn::WritePtzPermission)
        .condition(new PtzCondition(Ptz::PresetsPtzCapability, false));

    factory.beginSubMenu();
    {
        factory(PtzSavePresetAction)
            .mode(DesktopMode)
            .flags(Scene | SingleTarget)
            .text(ContextMenu::tr("Save Current Position..."))
            .requiredTargetPermissions(Qn::WritePtzPermission | Qn::SavePermission)
            .condition(ConditionWrapper(new PtzCondition(Ptz::PresetsPtzCapability, true))
                && condition::canSavePtzPosition());

        factory(PtzManageAction)
            .mode(DesktopMode)
            .flags(Scene | SingleTarget)
            .text(ContextMenu::tr("Manage..."))
            .requiredTargetPermissions(Qn::WritePtzPermission | Qn::SavePermission)
            .condition(ConditionWrapper(new PtzCondition(Ptz::ToursPtzCapability, false))
                && !condition::tourIsRunning());

    } factory.endSubMenu();

    factory(StartSmartSearchAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Show Motion/Smart Search"))
        .conditionalText(ContextMenu::tr("Show Motion"), new NoArchiveCondition())
        .condition(ConditionWrapper(new SmartSearchCondition(false))
            && !condition::isLayoutTourReviewMode());

    // TODO: #ynikitenkov remove this action, use StartSmartSearchAction with .checked state!
    factory(StopSmartSearchAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Hide Motion/Smart Search"))
        .conditionalText(ContextMenu::tr("Hide Motion"), new NoArchiveCondition())
        .condition(ConditionWrapper(new SmartSearchCondition(true))
            && !condition::isLayoutTourReviewMode());

    factory(ClearMotionSelectionAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Clear Motion Selection"))
        .condition(ConditionWrapper(new ClearMotionSelectionCondition())
            && !condition::tourIsRunning()
            && !condition::isLayoutTourReviewMode());

    factory(ToggleSmartSearchAction)
        .flags(Scene | SingleTarget | MultiTarget | HotkeyOnly)
        .shortcut("Alt+G")
        .condition(ConditionWrapper(new SmartSearchCondition())
            && !condition::isLayoutTourReviewMode());

    factory(CheckFileSignatureAction)
        .flags(Scene | SingleTarget)
        .text(ContextMenu::tr("Check File Watermark"))
        .shortcut(lit("Alt+C"))
        .autoRepeat(false)
        .condition(condition::hasFlags(Qn::local_video, MatchMode::any)
            && !condition::tourIsRunning()
            && !condition::isLayoutTourReviewMode());

    factory(TakeScreenshotAction)
        .flags(Scene | SingleTarget | HotkeyOnly)
        .shortcut(lit("Alt+S"))
        .autoRepeat(false)
        .condition(new TakeScreenshotCondition());

    factory(AdjustVideoAction)
        .flags(Scene | SingleTarget)
        .text(ContextMenu::tr("Image Enhancement..."))
        .shortcut(lit("Alt+J"))
        .autoRepeat(false)
        .condition(ConditionWrapper(new AdjustVideoCondition())
            && !condition::isLayoutTourReviewMode());

    factory(CreateZoomWindowAction)
        .flags(SingleTarget | WidgetTarget)
        .condition(ConditionWrapper(new CreateZoomWindowCondition())
            && !condition::tourIsRunning());

    factory(RotateToAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
        .text(ContextMenu::tr("Rotate to"))
        .condition(new RotateItemCondition())
        .childFactory(new RotateActionFactory(manager));

    factory(RadassAction)
        .flags(Scene | NoTarget | SingleTarget | MultiTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Resolution..."))
        .childFactory(new RadassActionFactory(manager))
        .condition(ConditionWrapper(new ChangeResolutionCondition())
            && !condition::isLayoutTourReviewMode());

    factory(MuteAction)
        .flags(Scene | SingleTarget | MultiTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Sound Playback..."))
        .childFactory(new SoundPlaybackActionFactory(manager))
        .condition(ConditionWrapper(new SoundPlaybackActionCondition()));

    factory(CreateNewCustomGroupAction)
        .mode(DesktopMode)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Create Group"))
        .shortcut(lit("Ctrl+G"))
        .autoRepeat(false)
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(
            (condition::treeNodeType(ResourceTree::NodeType::resource)
            || condition::treeNodeType(ResourceTree::NodeType::customResourceGroup)
            || condition::treeNodeType(ResourceTree::NodeType::recorder))
            && ConditionWrapper(new CreateNewResourceTreeGroupCondition()));

    factory(AssignCustomGroupIdAction)
        .mode(DesktopMode)
        .flags(SingleTarget | MultiTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(
            ConditionWrapper(new AssignResourceTreeGroupIdCondition()));

    factory(MoveToCustomGroupAction)
        .mode(DesktopMode)
        .flags(SingleTarget | MultiTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(
            ConditionWrapper(new MoveResourceTreeGroupIdCondition()));

    factory(RenameCustomGroupAction)
        .mode(DesktopMode)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Rename"))
        .requiredGlobalPermission(GlobalPermission::admin)
        .shortcut(lit("F2"))
        .autoRepeat(false)
        .condition(
            condition::treeNodeType(ResourceTree::NodeType::customResourceGroup)
            && ConditionWrapper(new RenameResourceTreeGroupCondition()));

    factory(RemoveCustomGroupAction)
        .mode(DesktopMode)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Remove Group"))
        .requiredGlobalPermission(GlobalPermission::admin)
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .autoRepeat(false)
        .condition(
            condition::treeNodeType(ResourceTree::NodeType::customResourceGroup)
            && ConditionWrapper(new RemoveResourceTreeGroupCondition()));

    factory()
        .flags(Scene | Tree | Table)
        .separator();

    factory(RemoveLayoutItemAction)
        .flags(Tree | SingleTarget | MultiTarget | LayoutItemTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Remove from Layout"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .autoRepeat(false)
        .condition(new LayoutItemRemovalCondition());

    factory(RemoveLayoutItemFromSceneAction)
        .flags(Scene | SingleTarget | MultiTarget | LayoutItemTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Remove from Layout"))
        .conditionalText(ContextMenu::tr("Remove from Showreel"),
            condition::isLayoutTourReviewMode())
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .autoRepeat(false)
        .condition(ConditionWrapper(new LayoutItemRemovalCondition())
            && !condition::tourIsRunning());

    factory(RemoveFromServerAction)
        .flags(Tree | Table | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .requiredTargetPermissions(Qn::RemovePermission)
        .text(ContextMenu::tr("Delete"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .autoRepeat(false)
        .condition(ConditionWrapper(new ResourceRemovalCondition()));

    factory(StopSharingLayoutAction)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .requiredGlobalPermission(GlobalPermission::admin)
        .text(ContextMenu::tr("Stop Sharing Layout"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .autoRepeat(false)
        .condition(new StopSharingCondition());

    factory()
        .flags(Scene | Tree)
        .separator();

    factory()
        .flags(Scene | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Page..."))
        .childFactory(new WebPageFactory(manager))
        .autoRepeat(false)
        .condition(condition::hasFlags(Qn::web_page, MatchMode::exactlyOne));

    factory(RenameResourceAction)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .requiredTargetPermissions(Qn::WritePermission | Qn::WriteNamePermission)
        .text(ContextMenu::tr("Rename"))
        .shortcut(lit("F2"))
        .autoRepeat(false)
        .condition(new RenameResourceCondition());

    factory(RenameVideowallEntityAction)
        .flags(Tree | SingleTarget | VideoWallItemTarget | VideoWallMatrixTarget | IntentionallyAmbiguous)
        .requiredGlobalPermission(GlobalPermission::controlVideowall)
        .text(ContextMenu::tr("Rename"))
        .shortcut(lit("F2"))
        .autoRepeat(false);

    factory()
        .flags(Tree | Table)
        .separator();

    factory(WebPageSettingsAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::admin)
        .text(ContextMenu::tr("Web Page Settings..."))
        .autoRepeat(false)
        .condition(condition::hasFlags(Qn::web_page, MatchMode::exactlyOne)
            && !condition::tourIsRunning());

    factory(DeleteFromDiskAction)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Delete from Disk"))
        .autoRepeat(false)
        .condition(condition::hasFlags(Qn::local_media, MatchMode::all)
            && condition::isTrue(ini().allowDeleteLocalFiles));

    factory(SetAsBackgroundAction)
        .flags(Scene | SingleTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission)
        .text(ContextMenu::tr("Set as Layout Background"))
        .autoRepeat(false)
        .condition(ConditionWrapper(new SetAsBackgroundCondition())
            && ConditionWrapper(new LightModeCondition(Qn::LightModeNoLayoutBackground))
            && !condition::tourIsRunning()
            && condition::applyToCurrentLayout(
                condition::hasFlags(Qn::cross_system, MatchMode::none)
            ));

    factory(UserSettingsAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("User Settings..."))
        .requiredTargetPermissions(Qn::ReadPermission)
        .condition(condition::hasFlags(Qn::user, MatchMode::any));

    factory(UserRolesAction)
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("User Roles..."))
        .conditionalText(
            ContextMenu::tr("Role Settings..."),
            condition::treeNodeType(ResourceTree::NodeType::role))
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(
            condition::treeNodeType(
                {ResourceTree::NodeType::users, ResourceTree::NodeType::role}));

    factory(UploadVirtualCameraFileAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::editCameras)
        .text(ContextMenu::tr("Upload File..."))
        .condition(condition::virtualCameraUploadEnabled());

    factory(UploadVirtualCameraFolderAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::editCameras)
        .text(ContextMenu::tr("Upload Folder..."))
        .condition(condition::virtualCameraUploadEnabled());

    factory(CancelVirtualCameraUploadsAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::editCameras)
        .text(ContextMenu::tr("Cancel Upload..."))
        .condition(condition::canCancelVirtualCameraUpload());

    factory(ReplaceCameraAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::admin)
        .text(ContextMenu::tr("Replace Camera..."))
        .condition(ConditionWrapper(new ReplaceCameraCondition())
            && condition::isTrue(ini().enableCameraReplacementFeature)
            && condition::scoped(SceneScope,
                !condition::isLayoutTourReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(UndoReplaceCameraAction)
        .flags(SingleTarget | ResourceTarget)
        .requiredGlobalPermission(GlobalPermission::admin)
        .mode(DesktopMode);

    factory(CameraIssuesAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | Table | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .dynamicText(new DevicesNameTextFactory(
            QnCameraDeviceStringSet(
                ContextMenu::tr("Check Device Issues..."), ContextMenu::tr("Check Devices Issues..."),
                ContextMenu::tr("Check Camera Issues..."), ContextMenu::tr("Check Cameras Issues..."),
                ContextMenu::tr("Check I/O Module Issues..."), ContextMenu::tr("Check I/O Modules Issues...")
            ), manager))
        .requiredGlobalPermission(GlobalPermission::viewLogs)
        .condition(
            condition::hasFlags(
                /*require*/ Qn::live_cam,
                /*exclude*/ Qn::cross_system | Qn::virtual_camera,
                MatchMode::all)
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope,
                !condition::isLayoutTourReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(CameraBusinessRulesAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | Table | SingleTarget | ResourceTarget | LayoutItemTarget)
        .dynamicText(new DevicesNameTextFactory(
            QnCameraDeviceStringSet(
                ContextMenu::tr("Device Rules..."),
                ContextMenu::tr("Camera Rules..."),
                ContextMenu::tr("I/O Module Rules...")
            ), manager))
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(
            condition::hasFlags(
                /*require*/ Qn::live_cam,
                /*exclude*/ Qn::cross_system | Qn::virtual_camera,
                MatchMode::any)
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope,
                !condition::isLayoutTourReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(CameraSettingsAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | Table | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .dynamicText(new DevicesNameTextFactory(
            QnCameraDeviceStringSet(
                ContextMenu::tr("Device Settings..."), ContextMenu::tr("Devices Settings..."),
                ContextMenu::tr("Camera Settings..."), ContextMenu::tr("Cameras Settings..."),
                ContextMenu::tr("I/O Module Settings..."), ContextMenu::tr("I/O Modules Settings...")
            ), manager))
        .requiredGlobalPermission(GlobalPermission::editCameras)
        .condition(condition::hasFlags(
                /*require*/ Qn::live_cam,
                /*exclude*/ Qn::cross_system,
                MatchMode::all)
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope,
                !condition::isLayoutTourReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(CopyRecordingScheduleAction)
        .mode(DesktopMode)
        .flags(NoTarget)
        .requiredGlobalPermission(GlobalPermission::editCameras);

    factory(MediaFileSettingsAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("File Settings..."))
        .condition(condition::hasFlags(Qn::local_media, MatchMode::any)
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope,
                !condition::isLayoutTourReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(LayoutSettingsAction)
        .mode(DesktopMode)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Layout Settings..."))
        .requiredTargetPermissions(Qn::EditLayoutSettingsPermission)
        .condition(ConditionWrapper(new LightModeCondition(Qn::LightModeNoLayoutBackground))
            && !condition::tourIsRunning());

    factory(VideowallSettingsAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Video Wall Settings..."))
        .condition(condition::hasFlags(Qn::videowall, MatchMode::exactlyOne)
            && ConditionWrapper(new AutoStartAllowedCondition())
        );

    factory(AnalyticsEngineSettingsAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Analytics Engine Settings..."))
        .condition(condition::isAnalyticsEngine());

    factory(AddDeviceManuallyAction)
        .flags(Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Add Device..."))   //intentionally hardcode devices here
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && ConditionWrapper(new EdgeServerCondition(false))
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(AddProxiedWebPageAction)
        .flags(Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Add Proxied Web Page..."))
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && ConditionWrapper(new EdgeServerCondition(false))
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(CameraListByServerAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->resourcePool(),
            ContextMenu::tr("Devices List by Server..."),
            ContextMenu::tr("Cameras List by Server...")
        ))
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && ConditionWrapper(new EdgeServerCondition(false))
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(PingAction)
        .flags(NoTarget);

    factory(ServerLogsAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Logs..."))
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode())
            && ConditionWrapper(new ResourceStatusCondition(
                nx::vms::api::ResourceStatus::online,
                true /* all */)));

    factory(ServerIssuesAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Diagnostics..."))
        .requiredGlobalPermission(GlobalPermission::viewLogs)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(WebAdminAction)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Web Page..."))
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && !ConditionWrapper(new FakeServerCondition(true))
            && !ConditionWrapper(new CloudServerCondition(MatchMode::any))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(ServerSettingsAction)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Settings..."))
        .requiredGlobalPermission(GlobalPermission::admin)
        .condition(
            condition::hasFlags(
                /*require*/ Qn::remote_server,
                /*exclude*/ Qn::cross_system,
                MatchMode::exactlyOne)
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(ConnectToCurrentSystem)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Merge to Currently Connected System..."))
        .condition(
            condition::treeNodeType(ResourceTree::NodeType::resource)
            && ConditionWrapper(new MergeToCurrentSystemCondition())
            && ConditionWrapper(new RequiresOwnerCondition())
        );

    factory()
        .flags(Scene | NoTarget)
        .childFactory(new AspectRatioFactory(manager))
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
        .text(ContextMenu::tr("Cell Aspect Ratio..."))
        .condition(!ConditionWrapper(new VideoWallReviewModeCondition())
            && ConditionWrapper(new LightModeCondition(Qn::LightModeSingleItem))
            && !condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    factory()
        .flags(Scene | NoTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
        .text(ContextMenu::tr("Cell Spacing"))
        .condition(ConditionWrapper(new LightModeCondition(Qn::LightModeSingleItem))
            && !condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    // TODO: #sivanov Move to childFactory, reduce actions number.
    factory.beginSubMenu();
    {
        factory.beginGroup();

        factory(SetCurrentLayoutItemSpacingNoneAction)
            .flags(Scene | NoTarget)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
            .text(ContextMenu::tr("None"))
            .checkable()
            .checked(QnWorkbenchLayout::kDefaultCellSpacing == Qn::CellSpacing::None);

        factory(SetCurrentLayoutItemSpacingSmallAction)
            .flags(Scene | NoTarget)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
            .text(ContextMenu::tr("Small"))
            .checkable()
            .checked(QnWorkbenchLayout::kDefaultCellSpacing == Qn::CellSpacing::Small);

        factory(SetCurrentLayoutItemSpacingMediumAction)
            .flags(Scene | NoTarget)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
            .text(ContextMenu::tr("Medium"))
            .checkable()
            .checked(QnWorkbenchLayout::kDefaultCellSpacing == Qn::CellSpacing::Medium);

        factory(SetCurrentLayoutItemSpacingLargeAction)
            .flags(Scene | NoTarget)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
            .text(ContextMenu::tr("Large"))
            .checkable()
            .checked(QnWorkbenchLayout::kDefaultCellSpacing == Qn::CellSpacing::Large);
        factory.endGroup();

    } factory.endSubMenu();

    factory()
        .flags(Scene)
        .separator();

#pragma region Layout Tours

    factory(ReviewLayoutTourAction)
        .flags(Tree | NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open in New Tab"))
        .condition(condition::treeNodeType(ResourceTree::NodeType::layoutTour))
        .autoRepeat(false);

    factory(ReviewLayoutTourInNewWindowAction)
        .flags(Tree | NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open in New Window"))
        .condition(condition::treeNodeType(ResourceTree::NodeType::layoutTour))
        .autoRepeat(false);

    factory().flags(Tree).separator().condition(
        condition::treeNodeType(ResourceTree::NodeType::layoutTour));

    factory(ToggleLayoutTourModeAction)
        .flags(Scene | Tree | NoTarget | GlobalHotkey)
        .mode(DesktopMode)
        .dynamicText(new LayoutTourTextFactory(manager))
        .shortcut(lit("Alt+T"))
        .checkable()
        .autoRepeat(false)
        .condition(
            condition::tourIsRunning()
            || (condition::treeNodeType(ResourceTree::NodeType::layoutTour) && condition::
                canStartTour()));

    factory(StartCurrentLayoutTourAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .text(LayoutTourTextFactory::tr("Start Showreel")) //< To be displayed on the button
        .accent(Qn::ButtonAccent::Standard)
        .icon(qnSkin->icon("buttons/play.png"))
        .condition(
            condition::isLayoutTourReviewMode()
            && ConditionWrapper(new StartCurrentLayoutTourCondition())
        )
        .autoRepeat(false);

    factory().flags(Tree).separator().condition(
        condition::treeNodeType(ResourceTree::NodeType::layoutTour));

    factory(RemoveLayoutTourAction)
        .flags(Tree | NoTarget | IntentionallyAmbiguous)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Delete"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .condition(condition::treeNodeType(ResourceTree::NodeType::layoutTour));

    factory().flags(Tree).separator().condition(
        condition::treeNodeType(ResourceTree::NodeType::layoutTour));

    factory(RenameLayoutTourAction)
        .flags(Tree | NoTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Rename"))
        .shortcut(lit("F2"))
        .condition(
            condition::treeNodeType(ResourceTree::NodeType::layoutTour)
        )
        .autoRepeat(false);

    factory(SaveLayoutTourAction)
        .flags(NoTarget)
        .mode(DesktopMode);

    factory(RemoveCurrentLayoutTourAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .condition(condition::isLayoutTourReviewMode())
        .autoRepeat(false);

    factory().flags(Tree).separator().condition(
        condition::treeNodeType(ResourceTree::NodeType::layoutTour));

    factory(LayoutTourSettingsAction)
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("Settings"))
        .condition(
            condition::treeNodeType(ResourceTree::NodeType::layoutTour)
        )
        .childFactory(new LayoutTourSettingsFactory(manager))
        .autoRepeat(false);

    factory()
        .flags(Scene)
        .separator();

    factory(CurrentLayoutTourSettingsAction)
        .flags(Scene | NoTarget)
        .text(ContextMenu::tr("Settings"))
        .condition(condition::isLayoutTourReviewMode())
        .childFactory(new LayoutTourSettingsFactory(manager))
        .autoRepeat(false);

#pragma endregion Layout Tours

    factory(CurrentLayoutSettingsAction)
        .flags(Scene | NoTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission)
        .text(ContextMenu::tr("Layout Settings..."))
        .conditionalText(ContextMenu::tr("Screen Settings..."),
            condition::currentLayoutIsVideowallScreen())
        .condition(ConditionWrapper(new LightModeCondition(Qn::LightModeNoLayoutBackground))
            && !condition::tourIsRunning());

    /* Tab bar actions. */
    factory()
        .flags(TitleBar)
        .separator();

    factory(CloseLayoutAction)
        .flags(GlobalHotkey | TitleBar | ScopelessHotkey | SingleTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Close"))
        .shortcut(lit("Ctrl+W"))
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(CloseAllButThisLayoutAction)
        .flags(TitleBar | SingleTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Close All But This"))
        .condition(new LayoutCountCondition(2));

    factory(DebugCalibratePtzAction)
        .flags(Scene | SingleTarget)
        .text("[Dev] Calibrate PTZ") //< Developer-only option, leave untranslatable.
        .condition(condition::hasFlags(
            /*require*/ Qn::live_cam,
            /*exclude*/ Qn::cross_system,
            MatchMode::any)
        && condition::isTrue(ini().calibratePtzActions));

    factory(DebugGetPtzPositionAction)
        .flags(Scene | SingleTarget)
        .text("[Dev] Get PTZ Position") //< Developer-only option, leave untranslatable.
        .condition(condition::hasFlags(
            /*require*/ Qn::live_cam,
            /*exclude*/ Qn::cross_system,
            MatchMode::any)
        && condition::isTrue(ini().calibratePtzActions));

    factory(PlayPauseAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
//        .shortcut(lit("Space")) - hotkey is handled directly in Main Window due to Qt issue
        .text(ContextMenu::tr("Play"))
        .toggledText(ContextMenu::tr("Pause"))
        .autoRepeat(false)
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::tourIsRunning());

    factory(PreviousFrameAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("Ctrl+Left"))
        .text(ContextMenu::tr("Previous Frame"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::tourIsRunning());

    factory(NextFrameAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("Ctrl+Right"))
        .text(ContextMenu::tr("Next Frame"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::tourIsRunning());

    factory(JumpToStartAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("Z"))
        .text(ContextMenu::tr("To Start"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::tourIsRunning());

    factory(JumpToEndAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("X"))
        .text(ContextMenu::tr("To End"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::tourIsRunning());

    factory(VolumeUpAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("Ctrl+Up"))
        .text(ContextMenu::tr("Volume Down"))
        .condition(new TimelineVisibleCondition());

    factory(VolumeDownAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("Ctrl+Down"))
        .text(ContextMenu::tr("Volume Up"))
        .condition(new TimelineVisibleCondition());

    factory(ToggleMuteAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("U"))
        .text(ContextMenu::tr("Toggle Mute"))
        .checkable()
        .condition(new TimelineVisibleCondition());

    factory(JumpToLiveAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("L"))
        .text(ContextMenu::tr("Jump to Live"))
        .condition(new ArchiveCondition());

    factory(ToggleSyncAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("S"))
        .text(ContextMenu::tr("Synchronize Streams"))
        .toggledText(ContextMenu::tr("Disable Stream Synchronization"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::tourIsRunning()
            && !condition::syncIsForced());

    factory(JumpToTimeAction)
        .flags(NoTarget)
        .condition(new TimelineVisibleCondition());

    factory()
        .flags(Slider | TitleBar | Tree)
        .separator();

    factory(ToggleThumbnailsAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Thumbnails"))
        .toggledText(ContextMenu::tr("Hide Thumbnails"));

    factory(BookmarksModeAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Bookmarks")) //< To be displayed on the button
        .requiredGlobalPermission(GlobalPermission::viewBookmarks)
        .toggledText(ContextMenu::tr("Hide Bookmarks"));

    factory(ToggleCalendarAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Calendar")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Hide Calendar"));

    factory(ToggleTitleBarAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Title Bar")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Hide Title Bar"))
        .condition(new ToggleTitleBarCondition());

    factory(PinCalendarAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Pin Calendar")) //< To be displayed on the button tooltip
        .toggledText(ContextMenu::tr("Unpin Calendar"))
        .checkable();

    factory(MinimizeDayTimeViewAction)
        .text(ContextMenu::tr("Minimize")) //< To be displayed on button tooltip
        .icon(qnSkin->icon("titlebar/dropdown.png"));

    if (ini().newPanelsLayout)
    {
        factory(ToggleLeftPanelAction)
            .flags(NoTarget)
            .text(ContextMenu::tr("Show Panel")) //< To be displayed on button tooltip
            .toggledText(ContextMenu::tr("Hide Panel"))
            .condition(condition::treeNodeType(ResourceTree::NodeType::root));
    }
    else
    {
        factory(ToggleTreeAction)
            .flags(NoTarget)
            .text(ContextMenu::tr("Show Tree")) //< To be displayed on button tooltip
            .toggledText(ContextMenu::tr("Hide Tree"))
            .condition(condition::treeNodeType(ResourceTree::NodeType::root));
    }

    factory(PinTimelineAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Pin"));

    factory(ToggleTimelineAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Timeline")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Hide Timeline"));

    factory(ToggleNotificationsAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Notifications")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Hide Notifications"));

    factory(GoToNextItemAction)
        .flags(NoTarget);

    factory(GoToPreviousItemAction)
        .flags(NoTarget);

    factory(GoToNextRowAction)
        .flags(NoTarget);

    factory(GoToPreviousRowAction)
        .flags(NoTarget);

    factory(RaiseCurrentItemAction)
        .flags(NoTarget);

    factory(GoToLayoutItemAction)
        .flags(SingleTarget | ResourceTarget | NoTarget)
        .condition(!condition::isLayoutTourReviewMode() && !condition::tourIsRunning());

    factory(ToggleCurrentItemMaximizationStateAction)
        .flags(NoTarget);

    factory(PtzContinuousMoveAction)
        .flags(NoTarget);

    factory(PtzActivatePresetByIndexAction)
        .flags(NoTarget);

    factory(PtzActivateByHotkeyAction)
        .flags(NoTarget);

    factory(PtzFocusInAction)
        .flags(NoTarget);

    factory(PtzFocusOutAction)
        .flags(NoTarget);

    factory(PtzFocusAutoAction)
        .flags(NoTarget);

    integrations::registerActions(&factory);

    factory(ExportFinishedEvent)
        .flags(NoTarget);

    factory(InitialResourcesReceivedEvent)
        .flags(NoTarget);

    factory(BookmarksPrefetchEvent)
        .flags(NoTarget);

    factory(WidgetAddedEvent)
        .flags(SingleTarget | WidgetTarget);

    factory(WatermarkCheckedEvent)
        .flags(NoTarget);

    factory(NewCustomGroupCreatedEvent)
        .flags(NoTarget);

    factory(CustomGroupIdAssignedEvent)
        .flags(NoTarget);

    factory(CustomGroupRenamedEvent)
        .flags(NoTarget);

    factory(CustomGroupRemovedEvent)
        .flags(NoTarget);

    factory(MovedToCustomGroupEvent)
        .flags(NoTarget);

    // -- Developer mode actions further. Please make note: texts are untranslatable. --

    factory()
        .flags(Main | DevMode)
        .separator();

    factory()
        .flags(Main | DevMode)
        .text("[Developer Mode]");

    factory.beginSubMenu();
    {
        factory(OpenNewSceneAction)
            .flags(GlobalHotkey | Main | DevMode)
            .text("Open New Scene")
            .shortcut("Ctrl+Shift+E")
            .autoRepeat(false);

        factory(OpenEventRulesDialogAction)
            .flags(GlobalHotkey | Main | DevMode)
            .mode(DesktopMode)
            .requiredGlobalPermission(GlobalPermission::admin)
            .text("New Event Rules...")
            .shortcut(lit("Ctrl+Alt+E"))
            .condition(!condition::tourIsRunning()
                && condition::hasNewEventRulesEngine())
            .autoRepeat(false);

        factory(ShowDebugOverlayAction)
            .flags(GlobalHotkey | Main | DevMode)
            .text("Show Debug Overlay")
            .shortcut("Ctrl+Alt+D")
            .autoRepeat(false);

        factory(ExportStandaloneClientAction)
            .flags(Main | DevMode)
            .text("Export Standalone Client")
            .condition(condition::isTrue(nx::build_info::isWindows()));

        factory(DebugControlPanelAction)
            .flags(GlobalHotkey | Main | DevMode)
            .shortcut("Ctrl+Alt+Shift+D")
            .text("Debug Control Panel");

        factory(DebugIncrementCounterAction)
            .flags(GlobalHotkey | Main | DevMode)
            .shortcut("Ctrl+Alt+Shift++")
            .text("Increment Debug Counter");

        factory(DebugDecrementCounterAction)
            .flags(GlobalHotkey | Main | DevMode)
            .shortcut("Ctrl+Alt+Shift+-")
            .text("Decrement Debug Counter");

    }
    factory.endSubMenu();

    // -- Developer mode actions end. Please do not add real actions afterwards.
}

std::string toString(IDType id)
{
    // We have a lot of dynamically generated actions with own ids, which do not have
    // meta-information.
    auto result = QMetaEnum::fromType<IDType>().valueToKey(id);
    return result
        ? std::string(result)
        : std::to_string(id);
}

bool fromString(const std::string_view& str, IDType* id)
{
    bool ok = false;
    *id = (IDType) QMetaEnum::fromType<IDType>().keyToValue(str.data(), &ok);
    return ok;
}

} // namespace nx::vms::client::desktop::ui::action
