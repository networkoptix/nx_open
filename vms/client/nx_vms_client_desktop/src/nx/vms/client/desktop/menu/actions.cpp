// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "actions.h"

#include <QtCore/QMetaEnum>

#include <client/client_runtime_settings.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/resource.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/integrations/integrations.h>
#include <nx/vms/client/desktop/radass/radass_action_factory.h>
#include <nx/vms/client/desktop/showreel/showreel_actions.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/timeline/timeline_actions_factory.h>
#include <ui/workbench/workbench_layout.h>

#include "action.h"
#include "action_conditions.h"
#include "action_factories.h"
#include "action_manager.h"
#include "action_text_factories.h"
#include "factories/rotate_action_factory.h"
#include "menu_factory.h"

namespace nx::vms::client::desktop::menu {

static const QColor kBasePrimaryColor = "#a5b7c0"; //< Value of light10 in default customization.
static const QColor kLight4Color = "#E1E7EA";
static const QColor kBaseWindowTextColor = "#698796"; //< Value of light16 ('windowText').

const core::SvgIconColorer::IconSubstitutions kTitleBarIconSubstitutions = {
    { QnIcon::Normal, {
        { kBasePrimaryColor, "light10" },
        { kLight4Color, "light4" },
        { kBaseWindowTextColor, "light16" },
    }},
    { QnIcon::Disabled, {
        { kBasePrimaryColor, "dark14" },
        { kLight4Color, "dark17" },
        { kBaseWindowTextColor, "light16" },
    }},
    { QnIcon::Selected, {
        { kBasePrimaryColor, "light4" },
        { kLight4Color, "light1" },
        { kBaseWindowTextColor, "light10" },
    }},
    { QnIcon::Active, {  //< Hovered
        { kBasePrimaryColor, "light4" },
        { kLight4Color, "light3" },
        { kBaseWindowTextColor, "light14" },
    }},
    { QnIcon::Error, {
        { kBasePrimaryColor, "red_l2" },
        { kLight4Color, "red_l3" },
        { kBaseWindowTextColor, "light16" },
    }},
};

static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kButtonsIconSubstitutions = {
    {QnIcon::Normal, {{kLight4Color, "light4"}}},
};

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
        .shortcut("Ctrl+Alt+F")
        .checkable();

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
        .shortcut("Ctrl+Tab");

    factory(PreviousLayoutAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .shortcut("Ctrl+Shift+Tab");

    factory(SelectAllAction)
        .flags(GlobalHotkey)
        .shortcut("Ctrl+A")
        .shortcutContext(Qt::WidgetWithChildrenShortcut);

    factory(SelectionChangeAction)
        .flags(NoTarget);

    factory(SelectNewItemAction)
        .flags(NoTarget | SingleTarget | ResourceTarget);

    factory(PreferencesLicensesTabAction)
        .flags(NoTarget)
        .requiredPowerUserPermissions();

    factory(PreferencesSmtpTabAction)
        .flags(NoTarget)
        .requiredPowerUserPermissions();

    factory(PreferencesNotificationTabAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Filter...")); //< To be displayed on button tooltip

    factory(PreferencesCloudTabAction)
        .flags(NoTarget)
        .requiredPowerUserPermissions();

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
        .shortcut("F11")
        .condition(!condition::showreelIsRunning());

    factory(FullscreenResourceAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .condition(!condition::showreelIsRunning());

    factory(ShowTimeLineOnVideowallAction)
        .flags(NoTarget)
        .mode(VideoWallMode)
        .condition(condition::isTrue(qnRuntime->videoWallWithTimeline()));

    factory(WhatsThisAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Help")) //< To be displayed on button tooltip
        .icon(qnSkin->icon("titlebar/window_question.svg"));

    factory(CameraDiagnosticsAction)
        .mode(DesktopMode)
        .flags(ResourceTarget | SingleTarget)
        .condition(
            condition::hasFlags(
                /*require*/ Qn::live_cam,
                /*exclude*/ Qn::cross_system,
                MatchMode::any)
            && !condition::showreelIsRunning());

    factory(OpenBusinessLogAction)
        .flags(NoTarget | SingleTarget | MultiTarget | ResourceTarget
            | LayoutItemTarget | WidgetTarget | GlobalHotkey)
        .mode(DesktopMode)
        .requiredGlobalPermission(GlobalPermission::viewLogs)
        .shortcut("Ctrl+L")
        .condition(!condition::showreelIsRunning())
        .text(ContextMenu::tr("Event Log...")); //< To be displayed on button tooltip

    factory(OpenBusinessRulesAction)
        .mode(DesktopMode)
        .flags(NoTarget | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .requiredPowerUserPermissions();

    factory(OpenFailoverPriorityAction)
        .mode(DesktopMode)
        .flags(NoTarget)
        .requiredPowerUserPermissions();

    factory(AcknowledgeEventAction)
        .mode(DesktopMode)
        .flags(NoTarget | SingleTarget | ResourceTarget)
        .condition(
            condition::hasPermissionsForResources(Qn::ManageBookmarksPermission)
            && condition::hasFlags(Qn::live_cam, MatchMode::all));

    factory(StartVideoWallControlAction)
        .flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget)
        .text(ContextMenu::tr("Control Video Wall"))
        .requiredTargetPermissions(Qn::VideoWallResourceRole, Qn::ReadWriteSavePermission)
        .condition(ConditionWrapper(new StartVideoWallControlCondition()));

    factory(PushMyScreenToVideowallAction)
        .flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget)
        .text(ContextMenu::tr("Push my screen"))
        .requiredTargetPermissions(Qn::VideoWallResourceRole, Qn::ReadWriteSavePermission)
        .condition(ConditionWrapper(new DesktopCameraCondition()));

    factory(QueueAppRestartAction)
        .flags(NoTarget);

    factory(SelectTimeServerAction)
        .flags(NoTarget)
        .requiredPowerUserPermissions()
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
        .shortcut("N")
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
        .shortcut("B")
        .condition(condition::isWorkbenchVisible())
        .text(ContextMenu::tr("Switch to Bookmarks tab"));

    factory(EventsTabAction)
        .flags(GlobalHotkey | HotkeyOnly)
        .shortcut("E")
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

    /* Context menu actions. */

    factory(FitInViewAction)
        .flags(Scene | NoTarget)
        .text(ContextMenu::tr("Fit in View"))
        .condition(!condition::isShowreelReviewMode()
            && !condition::showreelIsRunning());

    factory()
        .flags(Scene)
        .separator();

    factory(MainMenuAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Main Menu")) //< To be displayed on button tooltip
        .shortcut(QString("Alt+Space"), Builder::Mac, true)
        .condition(!condition::showreelIsRunning());

    factory(OpenLoginDialogAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Connect to Server..."))
        .conditionalText(ContextMenu::tr("Connect to Another Server..."),
            condition::isLoggedIn())
        .shortcut("Ctrl+Shift+C")
        .condition(!condition::showreelIsRunning());

    factory(DisconnectAction)
        .flags(NoTarget);

    factory(DisconnectMainMenuAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Disconnect from Server"))
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
            .text(ContextMenu::tr("Layout"))
            .pulledText(ContextMenu::tr("New Layout"))
            .shortcut("Ctrl+T")
            .condition(!condition::showreelIsRunning())
            .icon(qnSkin->icon("titlebar/plus_16.svg",
                nullptr,
                nullptr,
                kTitleBarIconSubstitutions));

        factory(OpenNewWindowAction)
            .flags(Main | GlobalHotkey)
            .mode(DesktopMode)
            .text(ContextMenu::tr("Window"))
            .pulledText(ContextMenu::tr("New Window"))
            .shortcut("Ctrl+N")
            .condition(condition::isLoggedIn()
                 && ConditionWrapper(new LightModeCondition(Qn::LightModeNoNewWindow)));

        factory(OpenWelcomeScreenAction)
            .flags(Main | GlobalHotkey)
            .mode(DesktopMode)
            .text(ContextMenu::tr("Welcome Screen"))
            .pulledText(ContextMenu::tr("New Welcome Screen"))
            .shortcut("Ctrl+Shift+T")
            .condition(new LightModeCondition(Qn::LightModeNoNewWindow));
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
        .icon(qnSkin->icon("titlebar/arrow_down_16.svg",
            nullptr,
            nullptr,
            kTitleBarIconSubstitutions));

    factory()
        .flags(TitleBar)
        .separator();

    factory(ShowServersInTreeAction)
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("Show Servers"))
        .checkable()
        .checked(false) //< This action will be kept in unchecked state.
        .condition(condition::isLoggedIn()
            && condition::treeNodeType({ResourceTree::NodeType::camerasAndDevices})
            && condition::allowedToShowServersInResourceTree());

    factory(HideServersInTreeAction)
        .flags(Tree | NoTarget | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Show Servers"))
        .checkable()
        .checked(true) //< This action will be kept in checked state.
        .condition(new HideServersInTreeCondition());

    factory(ToggleProxiedResources)
        .flags(Tree | NoTarget | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Show Proxied Resources"))
        .checkable()
        .checked(false) //< The actual state is set in ResourceTreeSettingsActionHandler.
        .condition(condition::isTrue(ini().webPagesAndIntegrations)
            && ConditionWrapper(new ToggleProxiedResourcesCondition));

    factory().separator()
        .flags(Tree);

    factory()
        .flags(Main | Tree | Scene)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open"))
        .condition(!condition::showreelIsRunning());

    factory.beginSubMenu();
    {
        factory(OpenFileAction)
            .flags(Main | Scene | NoTarget | GlobalHotkey)
            .mode(DesktopMode)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
            .text(ContextMenu::tr("Files..."))
            .shortcut("Ctrl+O")
            .condition(!condition::showreelIsRunning());

        factory(OpenFolderAction)
            .flags(Main | Scene)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
            .condition(!condition::showreelIsRunning())
            .text(ContextMenu::tr("Folder..."));

        factory().separator()
            .flags(Main);

        factory(WebClientAction)
            .flags(Main | Tree | NoTarget)
            .text(ContextMenu::tr("Web Client..."))
            .pulledText(ContextMenu::tr("Open Web Client..."))
            .condition(condition::isLoggedIn()
                && condition::treeNodeType({ResourceTree::NodeType::currentSystem,
                    ResourceTree::NodeType::servers, ResourceTree::NodeType::camerasAndDevices}));

    } factory.endSubMenu();

    factory(SaveCurrentShowreelAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .condition(condition::isShowreelReviewMode());

    factory(SaveCurrentLayoutAction)
        .mode(DesktopMode)
        .flags(Scene | NoTarget | GlobalHotkey | IntentionallyAmbiguous)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::SavePermission)
        .text(ContextMenu::tr("Save Current Layout"))
        .shortcut("Ctrl+S")
        .condition(ConditionWrapper(new SaveLayoutCondition(true))
            && !condition::isShowreelReviewMode()
            && !condition::showreelIsRunning());

    factory(SaveCurrentLayoutAsAction)
        .mode(DesktopMode)
        .flags(Scene | NoTarget | GlobalHotkey)
        .text(ContextMenu::tr("Save Current Layout As..."))
        .shortcut("Ctrl+Shift+S")
        .shortcut(QKeySequence("Ctrl+Alt+S"), Builder::Windows, false)
        .condition(
            condition::isLoggedIn()
            && condition::applyToCurrentLayout(condition::canSaveLayoutAs())
            && !condition::isShowreelReviewMode()
            && !condition::showreelIsRunning());

    factory(SaveCurrentLayoutAsCloudAction)
        .mode(DesktopMode)
        .flags(Scene | NoTarget)
        .text(ContextMenu::tr("Save Current Layout As Cloud..."))
        .condition(
            condition::isLoggedIn()
            && condition::isLoggedInToCloud()
            && condition::applyToCurrentLayout(
                condition::canSaveLayoutAs()
                && condition::hasFlags(Qn::cross_system, MatchMode::none))
            && !condition::isShowreelReviewMode()
            && !condition::showreelIsRunning());

    factory(SaveCurrentVideoWallReviewAction)
        .flags(Main | Scene | NoTarget | GlobalHotkey | IntentionallyAmbiguous)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Save Video Wall View"))
        .shortcut("Ctrl+S")
        .requiredTargetPermissions(Qn::ReadWriteSavePermission)
        .condition(ConditionWrapper(new SaveVideowallReviewCondition(true)));

    factory(DropOnVideoWallItemAction)
        .flags(ResourceTarget | LayoutItemTarget | LayoutTarget | VideoWallItemTarget | SingleTarget | MultiTarget);

    factory()
        .flags(Main)
        .separator();

    if (nx::build_info::isWindows() && qnRuntime->isDesktopMode())
    {
        factory(ToggleScreenRecordingAction)
            .flags(Main | GlobalHotkey)
            .mode(DesktopMode)
            .text(ContextMenu::tr("Start Screen Recording"))
            .toggledText(ContextMenu::tr("Stop Screen Recording"))
            .shortcut("Alt+R")
            .shortcut(Qt::Key_MediaRecord)
            .shortcutContext(Qt::ApplicationShortcut);

        factory()
            .flags(Main)
            .separator();
    }

    factory(EscapeHotkeyAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .shortcut("Esc")
        .text(ContextMenu::tr("Stop current action"));

    factory(FullscreenAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Go to Fullscreen"))
        .toggledText(ContextMenu::tr("Exit Fullscreen"))
        .shortcut(QKeySequence::FullScreen, Builder::Mac, true)
        .shortcutContext(Qt::ApplicationShortcut)
        .icon(qnSkin->icon("titlebar/window_maximize.svg", "titlebar/window_restore.svg"));

    factory(MinimizeAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Minimize"))
        .icon(qnSkin->icon("titlebar/window_minimize.svg"));

    factory(MaximizeAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Maximize"))
        .toggledText(ContextMenu::tr("Restore Down"))
        .icon(qnSkin->icon("titlebar/window_maximize.svg", "titlebar/window_restore.svg"));

    factory(FullscreenMaximizeHotkeyAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .shortcut("Alt+Enter")
        .shortcut("Alt+Return")
        .shortcutContext(Qt::ApplicationShortcut)
        .condition(PreventWhenFullscreenTransition::condition());

    factory(VersionMismatchMessageAction)
        .flags(NoTarget)
        .requiredPowerUserPermissions();

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
        .requiredPowerUserPermissions();

    factory(BrowseUrlAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open in Browser..."));

    factory(SystemAdministrationAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("System Administration..."))
        .shortcut("Ctrl+Alt+A")
        .requiredPowerUserPermissions()
        .condition(
            condition::treeNodeType({ResourceTree::NodeType::currentSystem,
                ResourceTree::NodeType::servers, ResourceTree::NodeType::camerasAndDevices})
            && !condition::showreelIsRunning());

    factory(SystemUpdateAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("System Update..."))
        .requiredPowerUserPermissions();

    factory(AdvancedUpdateSettingsAction)
        .flags(NoTarget)
        .requiredPowerUserPermissions();

    factory(UserManagementAction)
        .flags(Main | Tree)
        .requiredPowerUserPermissions()
        .text(ContextMenu::tr("User Management..."))
        .condition(condition::treeNodeType(ResourceTree::NodeType::users));

    factory(OpenLookupListsDialogAction)
        .flags(Main)
        .requiredPowerUserPermissions()
        .text(ContextMenu::tr("Lists Management..."))
        .condition(condition::isTrue(ini().lookupLists));

    factory(LogsManagementAction)
        .flags(NoTarget)
        .requiredPowerUserPermissions();

    factory(PreferencesGeneralTabAction)
        .flags(Main)
        .text(ContextMenu::tr("Local Settings..."))
        .role(QAction::PreferencesRole);

    factory(JoystickSettingsAction)
        .flags(Main)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Joystick Settings..."))
        .condition(condition::joystickConnected());

    factory(OpenAuditLogAction)
        .flags(Main)
        .requiredPowerUserPermissions()
        .text(ContextMenu::tr("Audit Trail..."));

    factory(OpenBookmarksSearchAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Bookmark Log..."))
        .shortcut("Ctrl+B")
        .condition(
            condition::isDeviceAccessRelevant(nx::vms::api::AccessRight::viewBookmarks)
            && !condition::showreelIsRunning());

    factory(OpenIntegrationsAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .requiredPowerUserPermissions()
        .text(ContextMenu::tr("Open Integrations..."));

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

    factory(ChangeDefaultCameraPasswordAction)
        .flags(SingleTarget | MultiTarget | ResourceTarget)
        .requiredPowerUserPermissions()
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
        .requiredPowerUserPermissions()
        .text(ContextMenu::tr("Event Rules..."))
        .shortcut("Ctrl+E")
        .condition(!condition::showreelIsRunning());

    factory(CameraListAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .requiredPowerUserPermissions()
        .text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->system()->resourcePool(),
            ContextMenu::tr("Devices List"),
            ContextMenu::tr("Cameras List")
        ))
        .shortcut("Ctrl+M")
        .condition(!condition::showreelIsRunning());

    factory()
        .flags(Main | Tree)
        .text(ContextMenu::tr("Add"));

    factory.beginSubMenu();
    {
        factory(MainMenuAddDeviceManuallyAction)
            .flags(Main)
            .text(ContextMenu::tr("Device..."))
            .requiredPowerUserPermissions();

        factory(NewUserAction)
            .flags(Main | Tree)
            .requiredPowerUserPermissions()
            .text(ContextMenu::tr("User..."))
            .pulledText(ContextMenu::tr("Add User..."))
            .condition(
                condition::treeNodeType(ResourceTree::NodeType::users)
            );

        factory(NewVideoWallAction)
            .flags(Main)
            .requiredPowerUserPermissions()
            .text(ContextMenu::tr("Video Wall..."));

        factory(NewIntegrationAction)
            .flags(Main | Tree)
            .requiredPowerUserPermissions()
            .text(ContextMenu::tr("Integration..."))
            .pulledText(ContextMenu::tr("New Integration..."))
            .condition(
                condition::treeNodeType(ResourceTree::NodeType::integrations)
                && condition::isTrue(ini().webPagesAndIntegrations)
            );

        factory(NewWebPageAction)
            .flags(Main | Tree)
            .requiredPowerUserPermissions()
            .text(ContextMenu::tr("Web Page..."))
            .pulledText(ini().webPagesAndIntegrations
                ? ContextMenu::tr("New Web Page...")
                : ContextMenu::tr("Add Web Page..."))
            .condition(
                condition::treeNodeType(ResourceTree::NodeType::webPages)
            );

        factory(NewShowreelAction)
            .flags(Main | Tree)
            .text(ContextMenu::tr("Showreel..."))
            .pulledText(ContextMenu::tr("Add Showreel..."))
            .condition(condition::isLoggedIn()
                && condition::treeNodeType(ResourceTree::NodeType::showreels)
            );

        factory(MainMenuAddVirtualCameraAction)
            .flags(Main)
            .requiredPowerUserPermissions()
            .text(ContextMenu::tr("Virtual Camera..."))
            .pulledText(ContextMenu::tr("Add Virtual Camera..."));
    }
    factory.endSubMenu();

    factory(MergeSystems)
        .flags(Main | Tree)
        .text(ContextMenu::tr("Merge Systems..."))
        .condition(
            condition::treeNodeType({ResourceTree::NodeType::currentSystem,
                ResourceTree::NodeType::servers, ResourceTree::NodeType::camerasAndDevices})
            && ConditionWrapper(new RequiresAdministratorCondition())
        );

    // TODO: Implement proxy actions to allow the same action to be shown in different locations.
    const auto systemAdministrationAction = manager->action(SystemAdministrationAction);
    const auto systemAdministrationAlias = factory.registerAction()
        .flags(Tree | NoTarget)
        .mode(DesktopMode)
        .text(systemAdministrationAction->text())
        .shortcut("Ctrl+Alt+A")
        .requiredPowerUserPermissions()
        .condition(
            condition::treeNodeType({ResourceTree::NodeType::currentSystem,
                ResourceTree::NodeType::servers, ResourceTree::NodeType::camerasAndDevices})
            && !condition::showreelIsRunning()).action();

    QObject::connect(systemAdministrationAlias, &QAction::triggered,
        systemAdministrationAction, &QAction::trigger);

    factory()
        .flags(Main)
        .separator();

    factory(OpenImportFromDevices)
        .flags(Main | NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Import From Devices..."))
        .condition(condition::isDeviceAccessRelevant(nx::vms::api::AccessRight::edit));

    factory()
        .flags(Main)
        .separator();

    factory(AboutAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("About..."))
        .shortcut("F1")
        .shortcutContext(Qt::ApplicationShortcut)
        .role(QAction::AboutRole)
        .condition(!condition::showreelIsRunning());

    factory(UserManualAction)
        .flags(Main)
        .mode(DesktopMode)
        .text(ContextMenu::tr("User Manual..."))
        .condition(!condition::showreelIsRunning());

    factory()
        .flags(Main)
        .separator();

    factory(SaveSessionState)
        .flags(Main)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Save Window Configuration"))
        .conditionalText(ContextMenu::tr("Save Windows Configuration"),
            condition::hasOtherWindowsInSession())
        .condition(condition::isLoggedIn() && !condition::hasSavedWindowsState());

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
        .shortcut("Alt+F4")
        .shortcut(QString("Ctrl+Q"), Builder::Mac, true)
        .shortcutContext(Qt::ApplicationShortcut)
        .role(QAction::QuitRole)
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
        .condition(condition::hasPermissionsForResources(Qn::ViewFootagePermission));

    factory()
        .flags(Slider)
        .separator();

    factory(StartTimeSelectionAction)
        .flags(Slider | SingleTarget)
        .text(ContextMenu::tr("Mark Selection Start"))
        .shortcut("[")
        .shortcutContext(Qt::WidgetShortcut)
        .condition(new TimePeriodCondition(NullTimePeriod, InvisibleAction));

    factory(EndTimeSelectionAction)
        .flags(Slider | SingleTarget)
        .text(ContextMenu::tr("Mark Selection End"))
        .shortcut("]")
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
        .requiredTargetPermissions(Qn::Permission::ManageBookmarksPermission)
        .condition(
            ConditionWrapper(new AddBookmarkCondition())
        );

    factory()
        .flags(Slider)
        .separator();

    factory(EditCameraBookmarkAction)
        .flags(Slider | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Edit Bookmark..."))
        .requiredTargetPermissions(Qn::Permission::ManageBookmarksPermission)
        .condition(
            ConditionWrapper(new ModifyBookmarkCondition())
        );

    factory(RemoveBookmarkAction)
        .flags(Slider | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Delete Bookmark..."))
        .requiredTargetPermissions(Qn::Permission::ManageBookmarksPermission)
        .condition(
            ConditionWrapper(new ModifyBookmarkCondition())
        );

    factory(RemoveBookmarksAction)
        .flags(MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Delete Bookmarks...")) //< Copied to an internal context menu
        .condition(
            condition::hasPermissionsForResources(Qn::ManageBookmarksPermission)
            && ConditionWrapper(new RemoveBookmarksCondition())
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
        .condition(condition::canCopyBookmarkToClipboard());

    factory(CopyBookmarksTextAction)
        .flags(MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Copy Bookmarks Text"))
        .condition(condition::canCopyBookmarksToClipboard());

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
            && !condition::isShowreelReviewMode());

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
        .requiredTargetPermissions(Qn::ReadWriteSavePermission)
        .condition(condition::hasFlags(Qn::videowall, MatchMode::any));

    factory(OpenInFolderAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Open Containing Folder"))
        .condition(new OpenInFolderCondition());

    factory(IdentifyVideoWallAction)
        .flags(Tree | Scene | SingleTarget | MultiTarget | ResourceTarget | VideoWallItemTarget)
        .text(ContextMenu::tr("Identify"))
        .condition(ConditionWrapper(new IdentifyVideoWallCondition()));

    factory(AttachToVideoWallAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .requiredPowerUserPermissions()
        .text(ContextMenu::tr("Attach to Video Wall..."))
        .condition(condition::hasFlags(Qn::videowall, MatchMode::any));

    factory(StartVideoWallAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Switch to Video Wall mode..."))
        .requiredTargetPermissions(Qn::ReadWriteSavePermission)
        .condition(ConditionWrapper(new StartVideowallCondition()));

    factory(SaveVideoWallReviewAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Save Video Wall"))
        .shortcut("Ctrl+S")
        .requiredTargetPermissions(Qn::ReadWriteSavePermission)
        .condition(ConditionWrapper(new SaveVideowallReviewCondition(false)));

    factory(SaveVideowallMatrixAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Save Current Matrix"))
        .requiredTargetPermissions(Qn::ReadWriteSavePermission)
        .condition(ConditionWrapper(new NonEmptyVideowallCondition()));

    factory(LoadVideowallMatrixAction)
        .flags(Tree | SingleTarget | VideoWallMatrixTarget)
        .requiredTargetPermissions(Qn::VideoWallResourceRole, Qn::ReadWriteSavePermission)
        .text(ContextMenu::tr("Load Matrix"));

    factory(DeleteVideowallMatrixAction)
        .flags(Tree | SingleTarget | MultiTarget | VideoWallMatrixTarget | IntentionallyAmbiguous)
        .requiredTargetPermissions(Qn::VideoWallResourceRole, Qn::ReadWriteSavePermission)
        .text(ContextMenu::tr("Delete"))
        .shortcut("Del")
        .shortcut(Qt::Key_Backspace, Builder::Mac, true);

    factory()
        .flags(Scene | Tree)
        .separator();

    factory(StopVideoWallAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Stop Video Wall"))
        .requiredTargetPermissions(Qn::ReadWriteSavePermission)
        .condition(condition::videowallIsRunning());

    factory(ClearVideoWallScreen)
        .flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget)
        .text(ContextMenu::tr("Clear Screen"))
        .requiredTargetPermissions(Qn::VideoWallResourceRole, Qn::ReadWriteSavePermission)
        .condition(ConditionWrapper(new DetachFromVideoWallCondition()));

    factory(SelectCurrentServerAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Connect to this Server"))
        .condition(new ReachableServerCondition());

    factory()
        .flags(Tree | VideoWallReviewScene | SingleTarget | VideoWallItemTarget)
        .separator();

    factory(VideoWallScreenSettingsAction)
        .flags(Tree | VideoWallReviewScene | SingleTarget | VideoWallItemTarget)
        .requiredTargetPermissions(Qn::VideoWallResourceRole, Qn::ReadWriteSavePermission)
        .text(ContextMenu::tr("Screen Settings..."));

    factory(ForgetLayoutPasswordAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Forget password"))
        .condition(condition::canForgetPassword() && !condition::isShowreelReviewMode());

    factory(SaveLayoutAction)
        .flags(TitleBar | Tree | SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::SavePermission)
        .dynamicText(new FunctionalTextFactory(
            [](const Parameters& parameters, WindowContext* /*context*/)
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
            [](const Parameters& parameters, WindowContext* /*context*/)
            {
                return parameters.resource()->hasFlags(Qn::cross_system)
                    ? ContextMenu::tr("Save Cloud Layout As...")
                    : ContextMenu::tr("Save Layout As...");
            },
            manager))
        .condition(
            condition::isLoggedIn()
            && condition::canSaveLayoutAs()
            && !condition::isShowreelReviewMode()
        );

    factory(SaveLayoutAsCloudAction)
        .flags(TitleBar | Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Save Layout As Cloud..."))
        .condition(
            condition::canSaveLayoutAs()
            && condition::isLoggedInToCloud()
            && condition::hasFlags(Qn::cross_system, MatchMode::none)
        );

    factory(ConvertLayoutToSharedAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Convert to Shared Layout"))
        .condition(condition::isOwnLayout())
        .requiredPowerUserPermissions();

    factory()
        .flags(Tree)
        .separator();

    factory(MakeShowreelAction)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Make Showreel"))
        .condition(condition::canMakeShowreel());

    factory()
        .flags(Scene | Tree)
        .separator();

    factory(DeleteVideoWallItemAction)
        .flags(Tree | SingleTarget | MultiTarget | VideoWallItemTarget | IntentionallyAmbiguous)
        .requiredPowerUserPermissions()
        .text(ContextMenu::tr("Delete"))
        .shortcut("Del")
        .shortcut(Qt::Key_Backspace, Builder::Mac, true);

    factory(MaximizeItemAction)
        .flags(Scene | SingleTarget)
        .text(ContextMenu::tr("Maximize Item"))
        .shortcut("Enter")
        .shortcut("Return")
        .condition(ConditionWrapper(new ItemZoomedCondition(false))
            && !condition::isShowreelReviewMode()
            && !condition::showreelIsRunning());

    factory(UnmaximizeItemAction)
        .flags(Scene | SingleTarget)
        .text(ContextMenu::tr("Restore Item"))
        .shortcut("Enter")
        .shortcut("Return")
        .condition(ConditionWrapper(new ItemZoomedCondition(true))
            && !condition::isShowreelReviewMode()
            && !condition::showreelIsRunning());

    factory()
        .flags(Scene | SingleTarget | MultiTarget)
        .childFactory(new ShowOnItemsFactory(manager))
        .dynamicText(new FunctionalTextFactory(
            [](const Parameters& parameters, WindowContext* /*context*/)
            {
                return ContextMenu::tr("Show on Items", "", parameters.resources().size());
            },
            manager));

    // Used via the hotkey.
    factory(ToggleInfoAction)
        .flags(Scene | SingleTarget | MultiTarget | HotkeyOnly)
        .shortcut("I")
        .shortcut("Alt+I")
        .autoRepeat(true)
        .condition(ConditionWrapper(new DisplayInfoCondition())
            && !condition::isShowreelReviewMode());

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
                && !condition::showreelIsRunning());

    } factory.endSubMenu();

    factory(StartSmartSearchAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Show Motion/Smart Search"))
        .conditionalText(ContextMenu::tr("Show Motion"), new NoArchiveCondition())
        .condition(ConditionWrapper(new SmartSearchCondition(false))
            && !condition::isShowreelReviewMode());

    // TODO: #ynikitenkov remove this action, use StartSmartSearchAction with .checked state!
    factory(StopSmartSearchAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Hide Motion/Smart Search"))
        .conditionalText(ContextMenu::tr("Hide Motion"), new NoArchiveCondition())
        .condition(ConditionWrapper(new SmartSearchCondition(true))
            && !condition::isShowreelReviewMode());

    factory(ClearMotionSelectionAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Clear Motion Selection"))
        .condition(ConditionWrapper(new ClearMotionSelectionCondition())
            && !condition::showreelIsRunning()
            && !condition::isShowreelReviewMode());

    factory(ToggleSmartSearchAction)
        .flags(Scene | SingleTarget | MultiTarget | HotkeyOnly)
        .shortcut("Alt+G")
        .autoRepeat(true)
        .condition(ConditionWrapper(new SmartSearchCondition())
            && !condition::isShowreelReviewMode());

    factory(CheckFileSignatureAction)
        .flags(Scene | SingleTarget)
        .text(ContextMenu::tr("Check File Watermark"))
        .shortcut("Alt+C")
        .condition(condition::hasFlags(Qn::local_video, MatchMode::any)
            && !condition::showreelIsRunning()
            && !condition::isShowreelReviewMode());

    factory(TakeScreenshotAction)
        .flags(Scene | SingleTarget | HotkeyOnly)
        .shortcut("Alt+S")
        .condition(new TakeScreenshotCondition());

    factory(AdjustVideoAction)
        .flags(Scene | SingleTarget)
        .text(ContextMenu::tr("Image Enhancement..."))
        .shortcut("Alt+J")
        .condition(ConditionWrapper(new AdjustVideoCondition())
            && !condition::isShowreelReviewMode());

    factory(CreateZoomWindowAction)
        .flags(SingleTarget | WidgetTarget)
        .condition(ConditionWrapper(new CreateZoomWindowCondition())
            && !condition::showreelIsRunning());

    factory(RotateToAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
        .text(ContextMenu::tr("Rotate to"))
        .condition(ConditionWrapper(new RotateItemCondition()) && !condition::layoutIsLocked())
        .childFactory(new RotateActionFactory(manager));

    factory(RadassAction)
        .flags(Scene | NoTarget | SingleTarget | MultiTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Resolution"))
        .childFactory(new RadassActionFactory(manager))
        .condition(ConditionWrapper(new ChangeResolutionCondition())
            && !condition::isShowreelReviewMode());

    factory(CreateNewCustomGroupAction)
        .mode(DesktopMode)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Create Group"))
        .shortcut("Ctrl+G")
        .requiredPowerUserPermissions()
        .condition(
            (condition::treeNodeType(ResourceTree::NodeType::resource)
            || condition::treeNodeType(ResourceTree::NodeType::customResourceGroup)
            || condition::treeNodeType(ResourceTree::NodeType::recorder))
            && ConditionWrapper(new CreateNewResourceTreeGroupCondition()));

    factory(AssignCustomGroupIdAction)
        .mode(DesktopMode)
        .flags(SingleTarget | MultiTarget | ResourceTarget)
        .requiredPowerUserPermissions()
        .condition(
            ConditionWrapper(new AssignResourceTreeGroupIdCondition()));

    factory(MoveToCustomGroupAction)
        .mode(DesktopMode)
        .flags(SingleTarget | MultiTarget | ResourceTarget)
        .requiredPowerUserPermissions()
        .condition(
            ConditionWrapper(new MoveResourceTreeGroupIdCondition()));

    factory(RenameCustomGroupAction)
        .mode(DesktopMode)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Rename"))
        .requiredPowerUserPermissions()
        .shortcut("F2")
        .condition(
            condition::treeNodeType(ResourceTree::NodeType::customResourceGroup)
            && ConditionWrapper(new RenameResourceTreeGroupCondition()));

    factory(RemoveCustomGroupAction)
        .mode(DesktopMode)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Remove Group"))
        .requiredPowerUserPermissions()
        .shortcut("Del")
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .condition(
            condition::treeNodeType(ResourceTree::NodeType::customResourceGroup)
            && ConditionWrapper(new RemoveResourceTreeGroupCondition()));

    factory()
        .flags(Scene | Tree | Table)
        .separator();

    factory(RemoveLayoutItemAction)
        .flags(Tree | SingleTarget | MultiTarget | LayoutItemTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Remove from Layout"))
        .shortcut("Del")
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .condition(new LayoutItemRemovalCondition());

    factory(ReplaceLayoutItemAction)
        .mode(DesktopMode)
        .flags(SingleTarget | WidgetTarget);

    factory(RemoveLayoutItemFromSceneAction)
        .flags(Scene | SingleTarget | MultiTarget | LayoutItemTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Remove from Layout"))
        .conditionalText(ContextMenu::tr("Remove from Showreel"),
            condition::isShowreelReviewMode())
        .shortcut("Del")
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .condition(ConditionWrapper(new LayoutItemRemovalCondition())
            && !condition::showreelIsRunning());

    factory(RemoveFromServerAction)
        .flags(Tree | Table | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .requiredTargetPermissions(Qn::RemovePermission)
        .text(ContextMenu::tr("Delete"))
        .shortcut("Del")
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .condition(ConditionWrapper(new ResourceRemovalCondition()));

    factory()
        .flags(Scene | Tree)
        .separator();

    factory()
        .flags(Scene | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Page..."))
        .childFactory(new WebPageFactory(manager))
        .condition(condition::hasFlags(Qn::web_page, MatchMode::exactlyOne));

    factory(RenameResourceAction)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .requiredTargetPermissions(Qn::WritePermission | Qn::WriteNamePermission)
        .text(ContextMenu::tr("Rename"))
        .shortcut("F2")
        .condition(new RenameResourceCondition());

    factory(RenameVideowallEntityAction)
        .flags(Tree | SingleTarget | VideoWallItemTarget | VideoWallMatrixTarget | IntentionallyAmbiguous)
        .requiredTargetPermissions(Qn::VideoWallResourceRole, Qn::WriteNamePermission)
        .text(ContextMenu::tr("Rename"))
        .shortcut("F2");

    factory()
        .flags(Tree | Table)
        .separator();

    if (ini().webPagesAndIntegrations)
    {
        factory(IntegrationSettingsAction)
            .flags(Scene | Tree | SingleTarget | ResourceTarget)
            .requiredPowerUserPermissions()
            .text(ContextMenu::tr("Integration Settings..."))
            .condition(condition::isIntegration() && !condition::showreelIsRunning());
    }

    factory(WebPageSettingsAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredPowerUserPermissions()
        .text(ContextMenu::tr("Web Page Settings..."))
        .condition(
            (ini().webPagesAndIntegrations
                ? condition::isWebPage()
                : condition::isWebPageOrIntegration())
            && !condition::showreelIsRunning());

    factory(DeleteFromDiskAction)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Delete from Disk"))
        .condition(condition::hasFlags(Qn::local_media, MatchMode::all)
            && condition::isTrue(ini().allowDeleteLocalFiles));

    factory(SetAsBackgroundAction)
        .flags(Scene | SingleTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission)
        .text(ContextMenu::tr("Set as Layout Background"))
        .condition(ConditionWrapper(new SetAsBackgroundCondition())
            && ConditionWrapper(new LightModeCondition(Qn::LightModeNoLayoutBackground))
            && !condition::showreelIsRunning()
            && condition::applyToCurrentLayout(
                condition::hasFlags(Qn::cross_system, MatchMode::none)
            ));

    factory(UserSettingsAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("User Settings..."))
        .requiredTargetPermissions(Qn::ReadPermission)
        .condition(condition::hasFlags(Qn::user, MatchMode::any));

    factory(UserGroupsAction)
        .flags(NoTarget)
        .requiredPowerUserPermissions();

    factory(UploadVirtualCameraFileAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::ReadWriteSavePermission)
        .text(ContextMenu::tr("Upload File..."))
        .condition(condition::virtualCameraUploadEnabled());

    factory(UploadVirtualCameraFolderAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::ReadWriteSavePermission)
        .text(ContextMenu::tr("Upload Folder..."))
        .condition(condition::virtualCameraUploadEnabled());

    factory(CancelVirtualCameraUploadsAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::ReadWriteSavePermission)
        .text(ContextMenu::tr("Cancel Upload..."))
        .condition(condition::canCancelVirtualCameraUpload());

    factory(ReplaceCameraAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredPowerUserPermissions()
        .text(ContextMenu::tr("Replace Camera..."))
        .condition(ConditionWrapper(new ReplaceCameraCondition())
            && condition::isTrue(ini().enableCameraReplacementFeature)
            && condition::scoped(SceneScope,
                !condition::isShowreelReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(UndoReplaceCameraAction)
        .flags(SingleTarget | ResourceTarget)
        .requiredPowerUserPermissions()
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
            && !condition::showreelIsRunning()
            && condition::scoped(SceneScope,
                !condition::isShowreelReviewMode()
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
        .requiredPowerUserPermissions()
        .condition(
            condition::hasFlags(
                /*require*/ Qn::live_cam,
                /*exclude*/ Qn::cross_system | Qn::virtual_camera,
                MatchMode::any)
            && !condition::showreelIsRunning()
            && condition::scoped(SceneScope,
                !condition::isShowreelReviewMode()
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
        .condition(
            condition::hasPermissionsForResources(
                Qn::ReadWriteSavePermission
                | Qn::WriteNamePermission)
            && condition::hasFlags(
                /*require*/ Qn::live_cam,
                /*exclude*/ Qn::cross_system,
                MatchMode::all)
            && !condition::showreelIsRunning()
            && condition::scoped(SceneScope,
                !condition::isShowreelReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(CopyRecordingScheduleAction)
        .mode(DesktopMode)
        .flags(NoTarget)
        .condition(condition::userHasCamerasWithEditableSettings());

    factory(MediaFileSettingsAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("File Settings..."))
        .condition(condition::hasFlags(Qn::local_media, MatchMode::any)
            && !condition::showreelIsRunning()
            && condition::scoped(SceneScope,
                !condition::isShowreelReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(LayoutSettingsAction)
        .mode(DesktopMode)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Layout Settings..."))
        .requiredTargetPermissions(Qn::EditLayoutSettingsPermission)
        .condition(ConditionWrapper(new LightModeCondition(Qn::LightModeNoLayoutBackground))
            && !condition::showreelIsRunning());

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

    factory()
        .flags(Main | Tree | SingleTarget | ResourceTarget)
        .text(ini().webPagesAndIntegrations
            ? ContextMenu::tr("New")
            : ContextMenu::tr("Add"))
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && ConditionWrapper(new EdgeServerCondition(false)));

    factory.beginSubMenu();
    {
        factory(AddDeviceManuallyAction)
            .flags(Tree | SingleTarget | ResourceTarget)
            .text(ContextMenu::tr("Device...")) //< Intentionally hardcode devices here.
            .requiredPowerUserPermissions();

        if (ini().webPagesAndIntegrations)
        {
            factory(AddProxiedIntegrationAction)
                .flags(Tree | SingleTarget | ResourceTarget)
                .text(ContextMenu::tr("Proxied Integration..."))
                .requiredPowerUserPermissions();
        }

        factory(AddProxiedWebPageAction)
            .flags(Tree | SingleTarget | ResourceTarget)
            .text(ContextMenu::tr("Proxied Web Page..."))
            .requiredPowerUserPermissions();

        factory(AddVirtualCameraAction)
            .flags(Tree | SingleTarget | ResourceTarget)
            .requiredPowerUserPermissions()
            .text(ContextMenu::tr("Virtual Camera..."));
    }
    factory.endSubMenu();

    factory()
        .flags(Tree)
        .separator();

    factory(CameraListByServerAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->system()->resourcePool(),
            ContextMenu::tr("Devices List by Server..."),
            ContextMenu::tr("Cameras List by Server...")
        ))
        .requiredPowerUserPermissions()
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && ConditionWrapper(new EdgeServerCondition(false))
            && !condition::showreelIsRunning()
            && condition::scoped(SceneScope, !condition::isShowreelReviewMode()));

    factory(PingAction)
        .flags(NoTarget);

    factory(ServerLogsAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Logs..."))
        .requiredPowerUserPermissions()
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && !condition::showreelIsRunning()
            && condition::scoped(SceneScope, !condition::isShowreelReviewMode())
            && ConditionWrapper(new ResourceStatusCondition(
                nx::vms::api::ResourceStatus::online,
                true /* all */)));

    factory(ServerIssuesAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Diagnostics..."))
        .requiredGlobalPermission(GlobalPermission::viewLogs)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && !condition::showreelIsRunning()
            && condition::scoped(SceneScope, !condition::isShowreelReviewMode()));

    factory(WebAdminAction)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Web Page..."))
        .requiredPowerUserPermissions()
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::exactlyOne)
            && !ConditionWrapper(new CloudServerCondition(MatchMode::any))
            && !condition::showreelIsRunning()
            && condition::scoped(SceneScope, !condition::isShowreelReviewMode()));

    factory(ServerSettingsAction)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Settings..."))
        .requiredPowerUserPermissions()
        .condition(
            condition::hasFlags(
                /*require*/ Qn::remote_server,
                /*exclude*/ Qn::cross_system,
                MatchMode::exactlyOne)
            && !condition::showreelIsRunning()
            && condition::scoped(SceneScope, !condition::isShowreelReviewMode()));

    factory(ConnectToCurrentSystem)
        // Actually, it is single-target, but the target type is not registered in Parameters.
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("Merge to Currently Connected System..."))
        .condition(
            condition::treeNodeType(ResourceTree::NodeType::otherSystemServer)
            && ConditionWrapper(new RequiresAdministratorCondition()));

    factory()
        .flags(Scene | NoTarget)
        .childFactory(new AspectRatioFactory(manager))
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
        .text(ContextMenu::tr("Cell Aspect Ratio"))
        .condition(!ConditionWrapper(new VideoWallReviewModeCondition())
            && ConditionWrapper(new LightModeCondition(Qn::LightModeSingleItem))
            && !condition::layoutIsLocked()
            && !condition::isShowreelReviewMode()
            && !condition::showreelIsRunning());

    factory()
        .flags(Scene | NoTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
        .text(ContextMenu::tr("Cell Spacing"))
        .condition(ConditionWrapper(new LightModeCondition(Qn::LightModeSingleItem))
            && !condition::isShowreelReviewMode()
            && !condition::showreelIsRunning()
            && !condition::layoutIsLocked());

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

#pragma region Showreels

    factory(ReviewShowreelAction)
        .flags(Tree | NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open in New Tab"))
        .condition(condition::treeNodeType(ResourceTree::NodeType::showreel));

    factory(ReviewShowreelInNewWindowAction)
        .flags(Tree | NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open in New Window"))
        .condition(condition::treeNodeType(ResourceTree::NodeType::showreel));

    factory().flags(Tree).separator().condition(
        condition::treeNodeType(ResourceTree::NodeType::showreel));

    factory(ToggleShowreelModeAction)
        .flags(Scene | Tree | NoTarget | GlobalHotkey)
        .mode(DesktopMode)
        .dynamicText(new ShowreelTextFactory(manager))
        .shortcut("Alt+T")
        .checkable()
        .condition(
            condition::showreelIsRunning()
            || (condition::treeNodeType(ResourceTree::NodeType::showreel)
                && condition::canStartShowreel()));

    factory(StartCurrentShowreelAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .text(ShowreelTextFactory::tr("Start Showreel")) //< To be displayed on the button
        .accent(Qn::ButtonAccent::Standard)
        .icon(qnSkin->icon("buttons/play_20.svg", kButtonsIconSubstitutions))
        .condition(
            condition::isShowreelReviewMode()
            && ConditionWrapper(new StartCurrentShowreelCondition())
        );

    factory().flags(Tree).separator().condition(
        condition::treeNodeType(ResourceTree::NodeType::showreel));

    factory(RemoveShowreelAction)
        .flags(Tree | NoTarget | IntentionallyAmbiguous)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Delete"))
        .shortcut("Del")
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .condition(condition::treeNodeType(ResourceTree::NodeType::showreel));

    factory().flags(Tree).separator().condition(
        condition::treeNodeType(ResourceTree::NodeType::showreel));

    factory(RenameShowreelAction)
        .flags(Tree | NoTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Rename"))
        .shortcut("F2")
        .condition(condition::treeNodeType(ResourceTree::NodeType::showreel));

    factory(SaveShowreelAction)
        .flags(NoTarget)
        .mode(DesktopMode);

    factory(RemoveCurrentShowreelAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .condition(condition::isShowreelReviewMode());

    factory(SuspendCurrentShowreelAction)
        .flags(NoTarget)
        .condition(condition::showreelIsRunning());

    factory(ResumeCurrentShowreelAction)
        .flags(NoTarget)
        .condition(condition::showreelIsRunning());

    factory()
        .flags(Tree)
        .separator()
        .condition(condition::treeNodeType(ResourceTree::NodeType::showreel));

    factory(ShowreelSettingsAction)
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("Settings"))
        .condition(condition::treeNodeType(ResourceTree::NodeType::showreel))
        .childFactory(new ShowreelSettingsFactory(manager));

    factory()
        .flags(Scene)
        .separator();

    factory(CurrentShowreelSettingsAction)
        .flags(Scene | NoTarget)
        .text(ContextMenu::tr("Settings"))
        .condition(condition::isShowreelReviewMode())
        .childFactory(new ShowreelSettingsFactory(manager));

#pragma endregion Showreels

    factory(CurrentLayoutSettingsAction)
        .flags(Scene | NoTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission)
        .text(ContextMenu::tr("Layout Settings..."))
        .conditionalText(ContextMenu::tr("Screen Settings..."),
            condition::currentLayoutIsVideowallScreen())
        .condition(ConditionWrapper(new LightModeCondition(Qn::LightModeNoLayoutBackground))
            && !condition::showreelIsRunning());

    /* Tab bar actions. */
    factory()
        .flags(TitleBar)
        .separator();

    factory(CloseLayoutAction)
        .flags(GlobalHotkey | TitleBar | ScopelessHotkey | SingleTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Close"))
        .shortcut("Ctrl+W")
        .condition(!condition::showreelIsRunning());

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
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::showreelIsRunning());

    factory(PreviousFrameAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut("Ctrl+Left")
        .autoRepeat(true)
        .text(ContextMenu::tr("Previous Frame"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::showreelIsRunning());

    factory(NextFrameAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut("Ctrl+Right")
        .autoRepeat(true)
        .text(ContextMenu::tr("Next Frame"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::showreelIsRunning());

    factory(JumpToStartAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut("Z")
        .text(ContextMenu::tr("To Start"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::showreelIsRunning());

    factory(JumpToEndAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut("X")
        .text(ContextMenu::tr("To End"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::showreelIsRunning());

    factory(VolumeUpAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut("Ctrl+Up")
        .autoRepeat(true)
        .text(ContextMenu::tr("Volume Up"))
        .condition(new TimelineVisibleCondition());

    factory(VolumeDownAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut("Ctrl+Down")
        .autoRepeat(true)
        .text(ContextMenu::tr("Volume Down"))
        .condition(new TimelineVisibleCondition());

    factory(ToggleMuteAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut("U")
        .text(ContextMenu::tr("Toggle Mute"))
        .checkable()
        .condition(new TimelineVisibleCondition());

    factory(JumpToLiveAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut("L")
        .text(ContextMenu::tr("Jump to Live"))
        .condition(new ArchiveCondition());

    factory(ToggleSyncAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut("S")
        .text(ContextMenu::tr("Synchronize Streams"))
        .toggledText(ContextMenu::tr("Disable Stream Synchronization"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::showreelIsRunning()
            && !condition::syncIsForced());

    factory(JumpToTimeAction)
        .flags(NoTarget)
        .condition(new TimelineVisibleCondition());

    factory(FastForwardAction)
        .flags(NoTarget)
        .condition(new TimelineVisibleCondition());

    factory(RewindAction)
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
        .toggledText(ContextMenu::tr("Hide Bookmarks"))
        .condition(condition::isDeviceAccessRelevant(nx::vms::api::AccessRight::viewBookmarks));

    factory(ToggleCalendarAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Calendar")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Hide Calendar"));

    factory(ToggleTitleBarAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Title Bar")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Hide Title Bar"))
        .condition(new ToggleTitleBarCondition());

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
        .condition(!condition::isShowreelReviewMode() && !condition::showreelIsRunning());

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
            .shortcut("Ctrl+Shift+E");

        factory(OpenVmsRulesDialogAction)
            .flags(GlobalHotkey | Main | DevMode)
            .mode(DesktopMode)
            .requiredPowerUserPermissions()
            .text("VMS Rules...")
            .shortcut("Ctrl+Alt+E")
            .condition(!condition::showreelIsRunning()
                && condition::hasNewEventRulesEngine());

        factory(OpenEventLogAction)
            .flags(GlobalHotkey | Main | DevMode)
            .mode(DesktopMode)
            .requiredGlobalPermission(GlobalPermission::viewLogs)
            .text("Event log...")
            .condition(!condition::showreelIsRunning()
                && condition::hasNewEventRulesEngine());

        factory(ShowDebugOverlayAction)
            .flags(GlobalHotkey | Main | DevMode)
            .text("Show Debug Overlay")
            .shortcut("Ctrl+Alt+D");

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
            .autoRepeat(true)
            .text("Increment Debug Counter");

        factory(DebugDecrementCounterAction)
            .flags(GlobalHotkey | Main | DevMode)
            .shortcut("Ctrl+Alt+Shift+-")
            .autoRepeat(true)
            .text("Decrement Debug Counter");

        factory(DebugToggleElementHighlight)
            .flags(GlobalHotkey | Main | DevMode)
            .shortcut("Ctrl+Alt+I") //< Same as Dev Tools in Chromium.
            .text("Toggle element highlight")
            .checkable();

        factory(DebugToggleSecurityForPowerUsersAction)
            .flags(Main | DevMode)
            .text("Toggle Security for Power Users")
            .checkable()
            .condition(ConditionWrapper(new RequiresAdministratorCondition()));
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

} // namespace nx::vms::client::desktop::menu
