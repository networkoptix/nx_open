#include "actions.h"

#include <core/resource/device_dependent_strings.h>

#include <ini.h>

#include <client/client_runtime_settings.h>

#include <nx/client/desktop/ui/actions/menu_factory.h>
#include <nx/client/desktop/ui/actions/action_conditions.h>
#include <nx/client/desktop/ui/actions/action_factories.h>
#include <nx/client/desktop/analytics/analytics_action_factory.h>
#include <nx/client/desktop/radass/radass_action_factory.h>
#include <nx/client/desktop/ui/actions/action_text_factories.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/layout_tour/layout_tour_actions.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/fusion/model_functions.h>

#include <nx/network/app_info.h>

#include <nx/utils/app_info.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::client::desktop::ui::action, IDType)

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class ContextMenu
{
    Q_DECLARE_TR_FUNCTIONS(ContextMenu)
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

    factory(ShowDebugOverlayAction)
        .flags(GlobalHotkey | DevMode)
        .text(lit("Show Debug")) //< DevMode, so untranslatable
        .shortcut(lit("Ctrl+Alt+D"))
        .autoRepeat(false);

    factory(DropResourcesAction)
        .flags(ResourceTarget | WidgetTarget | LayoutItemTarget | LayoutTarget | SingleTarget | MultiTarget)
        .mode(DesktopMode);

    factory(DelayedOpenVideoWallItemAction)
        .flags(NoTarget);

    factory(DelayedDropResourcesAction)
        .flags(NoTarget)
        .mode(DesktopMode);

    factory(InstantDropResourcesAction)
        .flags(NoTarget)
        .mode(DesktopMode);

    factory(MoveCameraAction)
        .flags(ResourceTarget | SingleTarget | MultiTarget)
        .requiredTargetPermissions(Qn::RemovePermission)
        .condition(condition::hasFlags(Qn::network, MatchMode::Any));

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
        .requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(PreferencesSmtpTabAction)
        .flags(NoTarget)
        .requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(PreferencesNotificationTabAction)
        .flags(NoTarget)
        .icon(qnSkin->icon("events/filter.png"))
        .text(ContextMenu::tr("Filter...")); //< To be displayed on button tooltip

    factory(PreferencesCloudTabAction)
        .flags(NoTarget)
        .requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(ConnectAction)
        .flags(NoTarget);

    factory(ConnectToCloudSystemAction)
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("Connect to System"))
        .condition(condition::treeNodeType(Qn::CloudSystemNode));

    factory(ReconnectAction)
        .flags(NoTarget);

    factory(FreespaceAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .shortcut(lit("F11"))
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(WhatsThisAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Help")) //< To be displayed on button tooltip
        .icon(qnSkin->icon("titlebar/window_question.png"));

    factory(CameraDiagnosticsAction)
        .mode(DesktopMode)
        .flags(ResourceTarget | SingleTarget)
        .condition(condition::hasFlags(Qn::live_cam, MatchMode::Any)
            && !condition::tourIsRunning());

    factory(OpenBusinessLogAction)
        .flags(NoTarget | SingleTarget | MultiTarget | ResourceTarget
            | LayoutItemTarget | WidgetTarget | GlobalHotkey)
        .mode(DesktopMode)
        .requiredGlobalPermission(Qn::GlobalViewLogsPermission)
        .icon(qnSkin->icon("events/log.png"))
        .shortcut(lit("Ctrl+L"))
        .condition(!condition::tourIsRunning())
        .text(ContextMenu::tr("Event Log...")); //< To be displayed on button tooltip

    factory(OpenBusinessRulesAction)
        .mode(DesktopMode)
        .flags(NoTarget | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(OpenFailoverPriorityAction)
        .mode(DesktopMode)
        .flags(NoTarget)
        .requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(OpenBackupCamerasAction)
        .mode(DesktopMode)
        .flags(NoTarget)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .text(ContextMenu::tr("Cameras to Backup..."));

    factory(StartVideoWallControlAction)
        .flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .text(ContextMenu::tr("Control Video Wall"))
        .condition(
            ConditionWrapper(new StartVideoWallControlCondition())
            && !condition::isSafeMode()
        );

    factory(PushMyScreenToVideowallAction)
        .flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .text(ContextMenu::tr("Push my screen"))
        .condition(
            ConditionWrapper(new DesktopCameraCondition())
            && !condition::isSafeMode()
        );

    factory(QueueAppRestartAction)
        .flags(NoTarget);

    factory(SelectTimeServerAction)
        .flags(NoTarget)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
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
        .shortcut(lit("Ctrl+Shift+C"))
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(DisconnectAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Disconnect from Server"))
        .autoRepeat(false)
        .shortcut(lit("Ctrl+Shift+D"))
        .condition(condition::isLoggedIn());

    factory(ResourcesModeAction)
        .flags(Main)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Browse Local Files"))
        .toggledText(ContextMenu::tr("Show Welcome Screen"))
        .condition(new BrowseLocalFilesCondition());

    factory()
        .flags(Main)
        .separator();

    factory(TogglePanicModeAction)
        .flags(GlobalHotkey | DevMode)
        .mode(DesktopMode)
        .text(lit("Start Panic Recording")) //< DevMode, so untranslatable
        .toggledText(lit("Stop Panic Recording")) //< DevMode, so untranslatable
        .autoRepeat(false)
        .shortcut(lit("Ctrl+P"))
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .condition(new PanicCondition());

    factory()
        .flags(Main | Tree)
        .separator();

    factory()
        .flags(Main | TitleBar | Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("New..."));

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
            .condition(new LightModeCondition(Qn::LightModeNoNewWindow));

        factory()
            .flags(Main)
            .separator();

        factory(NewUserAction)
            .flags(Main | Tree)
            .requiredGlobalPermission(Qn::GlobalAdminPermission)
            .text(ContextMenu::tr("User..."))
            .pulledText(ContextMenu::tr("New User..."))
            .condition(
                condition::treeNodeType(Qn::UsersNode)
                && !condition::isSafeMode()
            )
            .autoRepeat(false);

        factory(NewVideoWallAction)
            .flags(Main)
            .requiredGlobalPermission(Qn::GlobalAdminPermission)
            .text(ContextMenu::tr("Video Wall..."))
            .pulledText(ContextMenu::tr("New Video Wall..."))
            .condition(!condition::isSafeMode())
            .autoRepeat(false);

        factory(NewWebPageAction)
            .flags(Main | Tree)
            .requiredGlobalPermission(Qn::GlobalAdminPermission)
            .text(ContextMenu::tr("Web Page..."))
            .pulledText(ContextMenu::tr("New Web Page..."))
            .condition(
                condition::treeNodeType(Qn::WebPagesNode)
                && !condition::isSafeMode()
            )
            .autoRepeat(false);

        factory(NewLayoutTourAction)
            .flags(Main | Tree | NoTarget)
            .text(ContextMenu::tr("Showreel..."))
            .pulledText(ContextMenu::tr("New Showreel..."))
            .condition(condition::isLoggedIn()
                && condition::treeNodeType({Qn::LayoutToursNode})
                && !condition::isSafeMode()
            )
            .autoRepeat(false);

    }
    factory.endSubMenu();

    factory(NewUserLayoutAction)
        .flags(Tree | SingleTarget | ResourceTarget | NoTarget)
        .text(ContextMenu::tr("New Layout..."))
        .condition(
            ConditionWrapper(new NewUserLayoutCondition())
            && !condition::isSafeMode()
        );

    factory(OpenCurrentUserLayoutMenu)
        .flags(TitleBar | SingleTarget | NoTarget)
        .text(ContextMenu::tr("Open Layout...")) //< To be displayed on button tooltip
        .childFactory(new OpenCurrentUserLayoutFactory(manager))
        .icon(qnSkin->icon("titlebar/dropdown.png"));

    factory(StartAnalyticsAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Start Analytics..."))
        .childFactory(new AnalyticsActionFactory(manager))
        .condition(AnalyticsActionFactory::condition());

    factory()
        .flags(TitleBar)
        .separator();

    factory()
        .flags(Main | Tree | Scene)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open..."))
        .condition(!condition::tourIsRunning());

    factory.beginSubMenu();
    {
        factory(OpenFileAction)
            .flags(Main | Scene | NoTarget | GlobalHotkey)
            .mode(DesktopMode)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
            .text(ContextMenu::tr("File(s)..."))
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
                && condition::treeNodeType({Qn::CurrentSystemNode, Qn::ServersNode}));

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
        .shortcut(lit("Ctrl+Shift+S"))
        .shortcut(lit("Ctrl+Alt+S"), Builder::Windows, false)
        .autoRepeat(false)
        .condition(condition::isLoggedIn()
            && ConditionWrapper(new SaveLayoutAsCondition(true))
            && !condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    factory(ShareLayoutAction)
        .mode(DesktopMode)
        .flags(SingleTarget | ResourceTarget)
        .autoRepeat(false)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .condition(!condition::isSafeMode());

    factory(SaveCurrentVideoWallReviewAction)
        .flags(Main | Scene | NoTarget | GlobalHotkey | IntentionallyAmbiguous)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Save Video Wall View"))
        .shortcut(lit("Ctrl+S"))
        .autoRepeat(false)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .condition(
            !condition::isSafeMode()
            && ConditionWrapper(new SaveVideowallReviewCondition(true))
        );

    factory(DropOnVideoWallItemAction)
        .flags(ResourceTarget | LayoutItemTarget | LayoutTarget | VideoWallItemTarget | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Drop Resources"))
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .condition(!condition::isSafeMode());

    factory()
        .flags(Main)
        .separator();

    const bool screenRecordingSupported = nx::utils::AppInfo::isWindows();
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
            .autoRepeat(false);

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
        .flags(NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Go to Fullscreen"))
        .toggledText(ContextMenu::tr("Exit Fullscreen"))
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
        .shortcut(lit("Ctrl+F"), Builder::Mac, true)
        .shortcutContext(Qt::ApplicationShortcut);

    factory(VersionMismatchMessageAction)
        .flags(NoTarget)
        .requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(BetaVersionMessageAction)
        .flags(NoTarget)
        .mode(DesktopMode);

    factory(AllowStatisticsReportMessageAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(BrowseUrlAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open in Browser..."));

    factory(SystemAdministrationAction)
        .flags(Main | Tree | GlobalHotkey)
        .mode(DesktopMode)
        .text(ContextMenu::tr("System Administration..."))
        .shortcut(lit("Ctrl+Alt+A"))
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .condition(condition::treeNodeType({Qn::CurrentSystemNode, Qn::ServersNode})
            && !condition::tourIsRunning());

    factory(SystemUpdateAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("System Update..."))
        .requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(UserManagementAction)
        .flags(Main | Tree)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .text(ContextMenu::tr("User Management..."))
        .condition(condition::treeNodeType(Qn::UsersNode));

    factory(PreferencesGeneralTabAction)
        .flags(Main)
        .text(ContextMenu::tr("Local Settings..."))
        //.shortcut(lit("Ctrl+P"))
        .role(QAction::PreferencesRole)
        .autoRepeat(false);

    factory(OpenAuditLogAction)
        .flags(Main)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .text(ContextMenu::tr("Audit Trail..."));

    factory(OpenBookmarksSearchAction)
        .flags(Main | GlobalHotkey)
        .mode(DesktopMode)
        .requiredGlobalPermission(Qn::GlobalViewBookmarksPermission)
        .text(ContextMenu::tr("Bookmark Log..."))
        .shortcut(lit("Ctrl+B"))
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(LoginToCloud)
        .flags(NoTarget)
        .text(ContextMenu::tr("Log in to %1...", "Log in to Nx Cloud").arg(nx::network::AppInfo::cloudName()));

    factory(LogoutFromCloud)
        .flags(NoTarget)
        .text(ContextMenu::tr("Log out from %1", "Log out from Nx Cloud").arg(nx::network::AppInfo::cloudName()));

    factory(OpenCloudMainUrl)
        .flags(NoTarget)
        .text(ContextMenu::tr("Open %1 Portal...", "Open Nx Cloud Portal").arg(nx::network::AppInfo::cloudName()));

    factory(OpenCloudViewSystemUrl)
        .flags(NoTarget);

    factory(OpenCloudManagementUrl)
        .flags(NoTarget)
        .text(ContextMenu::tr("Account Settings..."));

    factory(HideCloudPromoAction)
        .flags(NoTarget);

    factory(ChangeDefaultCameraPasswordAction)
        .flags(SingleTarget | MultiTarget | ResourceTarget)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
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
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .text(ContextMenu::tr("Event Rules..."))
        .icon(qnSkin->icon("events/settings.png"))
        .shortcut(lit("Ctrl+E"))
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(CameraListAction)
        .flags(GlobalHotkey)
        .mode(DesktopMode)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->resourcePool(),
            ContextMenu::tr("Devices List"),
            ContextMenu::tr("Cameras List")
        ))
        .shortcut(lit("Ctrl+M"))
        .condition(!condition::tourIsRunning())
        .autoRepeat(false);

    factory(MergeSystems)
        .flags(Main | Tree)
        .text(ContextMenu::tr("Merge Systems..."))
        .condition(
            condition::treeNodeType({Qn::CurrentSystemNode, Qn::ServersNode})
            && !condition::isSafeMode()
            && ConditionWrapper(new RequiresOwnerCondition())
        );

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

    factory()
        .flags(Main)
        .separator();

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

    /* Slider actions. */
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

    factory(AcknowledgeEventAction)
        .flags(SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::ViewContentPermission)
        .requiredGlobalPermission(Qn::GlobalManageBookmarksPermission)
        .condition(condition::hasFlags(Qn::live_cam, MatchMode::ExactlyOne)
            && !condition::isSafeMode());

    factory(AddCameraBookmarkAction)
        .flags(Slider | SingleTarget)
        .text(ContextMenu::tr("Add Bookmark..."))
        .requiredGlobalPermission(Qn::GlobalManageBookmarksPermission)
        .condition(
            !condition::isSafeMode()
            && ConditionWrapper(new AddBookmarkCondition())
        );

    factory(EditCameraBookmarkAction)
        .flags(Slider | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Edit Bookmark..."))
        .requiredGlobalPermission(Qn::GlobalManageBookmarksPermission)
        .condition(
            !condition::isSafeMode()
            && ConditionWrapper(new ModifyBookmarkCondition())
        );

    factory(RemoveCameraBookmarkAction)
        .flags(Slider | SingleTarget)
        .text(ContextMenu::tr("Remove Bookmark..."))
        .requiredGlobalPermission(Qn::GlobalManageBookmarksPermission)
        .condition(
            !condition::isSafeMode()
            && ConditionWrapper(new ModifyBookmarkCondition())
        );

    factory(RemoveBookmarksAction)
        .flags(NoTarget | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Remove Bookmarks...")) //< Copied to an internal context menu
        .requiredGlobalPermission(Qn::GlobalManageBookmarksPermission)
        .condition(
            !condition::isSafeMode()
            && ConditionWrapper(new RemoveBookmarksCondition())
        );

    factory()
        .flags(Slider)
        .separator();

    factory(ExportVideoAction)
        .flags(Slider | SingleTarget | MultiTarget | NoTarget | WidgetTarget | ResourceTarget)
        .text(ContextMenu::tr("Export Video..."))
        .conditionalText(ContextMenu::tr("Export Bookmark..."),
            condition::hasArgument(Qn::CameraBookmarkRole))
        .requiredTargetPermissions(Qn::ExportPermission)
        .condition((ConditionWrapper(new ExportCondition(true))
            || condition::hasArgument(Qn::CameraBookmarkRole))
            && condition::isTrue(nx::client::desktop::ini().universalExportDialog));

    factory(ExportTimeSelectionAction)
        .flags(Slider | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Export Selected Area..."))
        .requiredTargetPermissions(Qn::ExportPermission)
        .condition((ConditionWrapper(new ExportCondition(true))
            || condition::hasArgument(Qn::CameraBookmarkRole))
            && !condition::isTrue(nx::client::desktop::ini().universalExportDialog));

    factory(ExportLayoutAction)
        .flags(Slider | SingleTarget | MultiTarget | NoTarget)
        .text(ContextMenu::tr("Export Multi-Video..."))
        .requiredTargetPermissions(Qn::CurrentLayoutMediaItemsRole, Qn::ExportPermission)
        .condition(ConditionWrapper(new ExportCondition(false))
            && !condition::isTrue(nx::client::desktop::ini().universalExportDialog));

    factory(ExportRapidReviewAction)
        .flags(Slider | SingleTarget | MultiTarget | NoTarget)
        .text(ContextMenu::tr("Export Rapid Review..."))
        .requiredTargetPermissions(Qn::CurrentLayoutMediaItemsRole, Qn::ExportPermission)
        .condition((ConditionWrapper(new ExportCondition(true))
            || condition::hasArgument(Qn::CameraBookmarkRole))
            && !condition::isTrue(nx::client::desktop::ini().universalExportDialog));

    factory(ThumbnailsSearchAction)
        .flags(Slider | Scene | SingleTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Preview Search..."))
        .condition(new PreviewCondition());

    factory()
        .flags(Tree | SingleTarget | ResourceTarget)
        .childFactory(new EdgeNodeFactory(manager))
        .text(ContextMenu::tr("Server..."))
        .condition(condition::treeNodeType(Qn::EdgeNode));

    factory()
        .flags(Scene | Tree)
        .separator();

    /* Resource actions. */
    factory(OpenInLayoutAction)
        .flags(SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .requiredTargetPermissions(Qn::LayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
        .condition(new OpenInLayoutCondition());

    factory(OpenInCurrentLayoutAction)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
        .text(ContextMenu::tr("Open"))
        .conditionalText(ContextMenu::tr("Monitor"),
            condition::hasFlags(Qn::server, MatchMode::All))
        .condition(
            ConditionWrapper(new OpenInCurrentLayoutCondition())
            && !condition::isLayoutTourReviewMode());

    factory(OpenInNewTabAction)
        .mode(DesktopMode)
        .flags(Tree | Scene | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .text(ContextMenu::tr("Open in New Tab"))
        .conditionalText(ContextMenu::tr("Monitor in New Tab"),
            condition::hasFlags(Qn::server, MatchMode::All))
        .condition(new OpenInNewEntityCondition());

    factory(OpenInAlarmLayoutAction)
        .mode(DesktopMode)
        .flags(SingleTarget | MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Open in Alarm Layout"));

    factory(OpenInNewWindowAction)
        .mode(DesktopMode)
        .flags(Tree | Scene | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget | WidgetTarget)
        .text(ContextMenu::tr("Open in New Window"))
        .conditionalText(ContextMenu::tr("Monitor in New Window"),
            condition::hasFlags(Qn::server, MatchMode::All))
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
        .condition(condition::hasFlags(Qn::videowall, MatchMode::Any));

    factory(OpenInFolderAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Open Containing Folder"))
        .shortcut(lit("Ctrl+Enter"))
        .shortcut(lit("Ctrl+Return"))
        .autoRepeat(false)
        .condition(new OpenInFolderCondition());

    factory(IdentifyVideoWallAction)
        .flags(Tree | Scene | SingleTarget | MultiTarget | ResourceTarget | VideoWallItemTarget)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .text(ContextMenu::tr("Identify"))
        .autoRepeat(false)
        .condition(new IdentifyVideoWallCondition());

    factory(AttachToVideoWallAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .text(ContextMenu::tr("Attach to Video Wall..."))
        .autoRepeat(false)
        .condition(
            !condition::isSafeMode()
            && condition::hasFlags(Qn::videowall, MatchMode::Any)
        );

    factory(StartVideoWallAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .text(ContextMenu::tr("Switch to Video Wall mode..."))
        .autoRepeat(false)
        .condition(new StartVideowallCondition());

    factory(SaveVideoWallReviewAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Save Video Wall"))
        .shortcut(lit("Ctrl+S"))
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .autoRepeat(false)
        .condition(
            ConditionWrapper(new SaveVideowallReviewCondition(false))
            && !condition::isSafeMode()
        );

    factory(SaveVideowallMatrixAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .text(ContextMenu::tr("Save Current Matrix"))
        .autoRepeat(false)
        .condition(
            ConditionWrapper(new NonEmptyVideowallCondition())
            && !condition::isSafeMode()
        );

    factory(LoadVideowallMatrixAction)
        .flags(Tree | SingleTarget | VideoWallMatrixTarget)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .condition(!condition::isSafeMode())
        .text(ContextMenu::tr("Load Matrix"));

    factory(DeleteVideowallMatrixAction)
        .flags(Tree | SingleTarget | MultiTarget | VideoWallMatrixTarget | IntentionallyAmbiguous)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .text(ContextMenu::tr("Delete"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .condition(!condition::isSafeMode())
        .autoRepeat(false);

    factory()
        .flags(Scene | Tree)
        .separator();

    factory(StopVideoWallAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .text(ContextMenu::tr("Stop Video Wall"))
        .autoRepeat(false)
        .condition(new RunningVideowallCondition());

    factory(ClearVideoWallScreen)
        .flags(Tree | VideoWallReviewScene | SingleTarget | MultiTarget | VideoWallItemTarget)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .text(ContextMenu::tr("Clear Screen"))
        .autoRepeat(false)
        .condition(new DetachFromVideoWallCondition());

    factory(SaveLayoutAction)
        .flags(TitleBar | Tree | SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::SavePermission)
        .text(ContextMenu::tr("Save Layout"))
        .condition(ConditionWrapper(new SaveLayoutCondition(false)));

    factory(SaveLocalLayoutAction)
        .flags(SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::SavePermission)
        .condition(condition::hasFlags(Qn::layout, MatchMode::All));

    factory(SaveLocalLayoutAsAction)
        .flags(SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::SavePermission)
        .condition(condition::hasFlags(Qn::layout, MatchMode::All));

    factory(SaveLayoutAsAction) // TODO: #GDM #access check canCreateResource permission
        .flags(SingleTarget | ResourceTarget)
        .requiredTargetPermissions(Qn::UserResourceRole, Qn::SavePermission)
        .condition(
            ConditionWrapper(new SaveLayoutAsCondition(false))
            && !condition::isLayoutTourReviewMode()
        );

    factory(SaveLayoutForCurrentUserAsAction) // TODO: #GDM #access check canCreateResource permission
        .flags(TitleBar | Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("Save Layout As..."))
        .condition(
            ConditionWrapper(new SaveLayoutAsCondition(false))
            && !condition::isLayoutTourReviewMode()
        );

    factory()
        .flags(Scene | Tree)
        .separator();

    factory(DeleteVideoWallItemAction)
        .flags(Tree | SingleTarget | MultiTarget | VideoWallItemTarget | IntentionallyAmbiguous)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .text(ContextMenu::tr("Delete"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .condition(!condition::isSafeMode())
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

    factory(ShowInfoAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Show Info"))
        .shortcut(lit("Alt+I"))
        .condition(ConditionWrapper(new DisplayInfoCondition(false))
            && !condition::isLayoutTourReviewMode());

    factory(HideInfoAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Hide Info"))
        .shortcut(lit("Alt+I"))
        .condition(ConditionWrapper(new DisplayInfoCondition(true))
            && !condition::isLayoutTourReviewMode());

    factory(ToggleInfoAction)
        .flags(Scene | SingleTarget | MultiTarget | HotkeyOnly)
        .shortcut(lit("Alt+I"))
        .condition(ConditionWrapper(new DisplayInfoCondition())
            && !condition::isLayoutTourReviewMode());

    factory()
        .flags(Scene | SingleTarget)
        .childFactory(new PtzPresetsToursFactory(manager))
        .text(ContextMenu::tr("PTZ..."))
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
        .shortcut(lit("Alt+G"))
        .condition(ConditionWrapper(new SmartSearchCondition(false))
            && !condition::isLayoutTourReviewMode());

    // TODO: #ynikitenkov remove this action, use StartSmartSearchAction with .checked state!
    factory(StopSmartSearchAction)
        .flags(Scene | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Hide Motion/Smart Search"))
        .conditionalText(ContextMenu::tr("Hide Motion"), new NoArchiveCondition())
        .shortcut(lit("Alt+G"))
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
        .shortcut(lit("Alt+G"))
        .condition(ConditionWrapper(new SmartSearchCondition())
            && !condition::isLayoutTourReviewMode());

    factory(CheckFileSignatureAction)
        .flags(Scene | SingleTarget)
        .text(ContextMenu::tr("Check File Watermark"))
        .shortcut(lit("Alt+C"))
        .autoRepeat(false)
        .condition(condition::hasFlags(Qn::local_video, MatchMode::Any)
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

    factory()
        .flags(Scene | SingleTarget | MultiTarget)
        .text(ContextMenu::tr("Rotate to..."));

    factory.beginSubMenu();
    {
        factory(Rotate0Action)
            .flags(Scene | SingleTarget | MultiTarget)
            .text(ContextMenu::tr("0 degrees"))
            .condition(new RotateItemCondition());

        factory(Rotate90Action)
            .flags(Scene | SingleTarget | MultiTarget)
            .text(ContextMenu::tr("90 degrees"))
            .condition(new RotateItemCondition());

        factory(Rotate180Action)
            .flags(Scene | SingleTarget | MultiTarget)
            .text(ContextMenu::tr("180 degrees"))
            .condition(new RotateItemCondition());

        factory(Rotate270Action)
            .flags(Scene | SingleTarget | MultiTarget)
            .text(ContextMenu::tr("270 degrees"))
            .condition(new RotateItemCondition());
    } factory.endSubMenu();

    factory(RadassAction)
        .flags(Scene | NoTarget | SingleTarget | MultiTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Resolution..."))
        .childFactory(new RadassActionFactory(manager))
        .condition(ConditionWrapper(new ChangeResolutionCondition())
            && !condition::isLayoutTourReviewMode());

    factory()
        .flags(Scene | Tree)
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
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .requiredTargetPermissions(Qn::RemovePermission)
        .text(ContextMenu::tr("Delete"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .autoRepeat(false)
        .condition(new ResourceRemovalCondition());

    factory(StopSharingLayoutAction)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .text(ContextMenu::tr("Stop Sharing Layout"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .autoRepeat(false)
        .condition(new StopSharingCondition());

    factory()
        .flags(Scene | Tree)
        .separator();

    factory(WebPageSettingsAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget)
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .text(ContextMenu::tr("Edit..."))
        .autoRepeat(false)
        .condition(condition::hasFlags(Qn::web_page, MatchMode::ExactlyOne)
            && !condition::isSafeMode()
            && !condition::tourIsRunning());

    factory(RenameResourceAction)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget | IntentionallyAmbiguous)
        .requiredTargetPermissions(Qn::WritePermission | Qn::WriteNamePermission)
        .text(ContextMenu::tr("Rename"))
        .shortcut(lit("F2"))
        .autoRepeat(false)
        .condition(new RenameResourceCondition());

    factory(RenameVideowallEntityAction)
        .flags(Tree | SingleTarget | VideoWallItemTarget | VideoWallMatrixTarget | IntentionallyAmbiguous)
        .requiredGlobalPermission(Qn::GlobalControlVideoWallPermission)
        .text(ContextMenu::tr("Rename"))
        .shortcut(lit("F2"))
        .condition(!condition::isSafeMode())
        .autoRepeat(false);

    factory()
        .flags(Tree)
        .separator();

    // TODO: #gdm restore this functionality and allow to delete exported layouts
    factory(DeleteFromDiskAction)
        //flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Delete from Disk"))
        .autoRepeat(false)
        .condition(condition::hasFlags(Qn::local_media, MatchMode::All));

    factory(SetAsBackgroundAction)
        .flags(Scene | SingleTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission)
        .text(ContextMenu::tr("Set as Layout Background"))
        .autoRepeat(false)
        .condition(ConditionWrapper(new SetAsBackgroundCondition())
            && ConditionWrapper(new LightModeCondition(Qn::LightModeNoLayoutBackground))
            && !condition::isSafeMode()
            && !condition::tourIsRunning());

    factory(UserSettingsAction)
        .flags(Tree | SingleTarget | ResourceTarget)
        .text(ContextMenu::tr("User Settings..."))
        .requiredTargetPermissions(Qn::ReadPermission)
        .condition(condition::hasFlags(Qn::user, MatchMode::Any));

    factory(UserRolesAction)
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("User Roles..."))
        .conditionalText(ContextMenu::tr("Role Settings..."), condition::treeNodeType(Qn::RoleNode))
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .condition(condition::treeNodeType({Qn::UsersNode, Qn::RoleNode}));

    factory(CameraIssuesAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .dynamicText(new DevicesNameTextFactory(
            QnCameraDeviceStringSet(
                ContextMenu::tr("Check Device Issues..."), ContextMenu::tr("Check Devices Issues..."),
                ContextMenu::tr("Check Camera Issues..."), ContextMenu::tr("Check Cameras Issues..."),
                ContextMenu::tr("Check I/O Module Issues..."), ContextMenu::tr("Check I/O Modules Issues...")
            ), manager))
        .requiredGlobalPermission(Qn::GlobalViewLogsPermission)
        .condition(condition::hasFlags(Qn::live_cam, MatchMode::Any)
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope,
                !condition::isLayoutTourReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(CameraBusinessRulesAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .dynamicText(new DevicesNameTextFactory(
            QnCameraDeviceStringSet(
                ContextMenu::tr("Device Rules..."), ContextMenu::tr("Devices Rules..."),
                ContextMenu::tr("Camera Rules..."), ContextMenu::tr("Cameras Rules..."),
                ContextMenu::tr("I/O Module Rules..."), ContextMenu::tr("I/O Modules Rules...")
            ), manager))
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .condition(condition::hasFlags(Qn::live_cam, MatchMode::ExactlyOne)
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope,
                !condition::isLayoutTourReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(CameraSettingsAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .dynamicText(new DevicesNameTextFactory(
            QnCameraDeviceStringSet(
                ContextMenu::tr("Device Settings..."), ContextMenu::tr("Devices Settings..."),
                ContextMenu::tr("Camera Settings..."), ContextMenu::tr("Cameras Settings..."),
                ContextMenu::tr("I/O Module Settings..."), ContextMenu::tr("I/O Modules Settings...")
            ), manager))
        .requiredGlobalPermission(Qn::GlobalEditCamerasPermission)
        .condition(condition::hasFlags(Qn::live_cam, MatchMode::Any)
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope,
                !condition::isLayoutTourReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(MediaFileSettingsAction)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("File Settings..."))
        .condition(condition::hasFlags(Qn::local_media, MatchMode::Any)
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
        .condition(condition::hasFlags(Qn::videowall, MatchMode::ExactlyOne)
            && ConditionWrapper(new AutoStartAllowedCondition())
            && !condition::isSafeMode());

    factory(ConvertCameraToEntropix)
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(lit("Convert to Entropix Camera"))
        .conditionalText(
            lit("Convert to Normal Camera"),
            condition::isEntropixCamera())
        .requiredGlobalPermission(Qn::GlobalEditCamerasPermission)
        .condition(condition::isTrue(nx::client::desktop::ini().enableEntropixEnhancer)
            && condition::hasFlags(Qn::live_cam, MatchMode::Any)
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope,
                !condition::isLayoutTourReviewMode()
                && !condition::isPreviewSearchMode()));

    factory(ServerAddCameraManuallyAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Add Device..."))   //intentionally hardcode devices here
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::ExactlyOne)
            && ConditionWrapper(new EdgeServerCondition(false))
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::isSafeMode()
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(CameraListByServerAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(QnDeviceDependentStrings::getDefaultNameFromSet(
            manager->resourcePool(),
            ContextMenu::tr("Devices List by Server..."),
            ContextMenu::tr("Cameras List by Server...")
        ))
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::ExactlyOne)
            && ConditionWrapper(new EdgeServerCondition(false))
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(PingAction)
        .flags(NoTarget);

    factory(ServerLogsAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Logs..."))
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::ExactlyOne)
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(ServerIssuesAction)
        .flags(Scene | Tree | SingleTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Diagnostics..."))
        .requiredGlobalPermission(Qn::GlobalViewLogsPermission)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::ExactlyOne)
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(WebAdminAction)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Web Page..."))
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::ExactlyOne)
            && !ConditionWrapper(new FakeServerCondition(true))
            && !ConditionWrapper(new CloudServerCondition(MatchMode::Any))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(ServerSettingsAction)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget | LayoutItemTarget)
        .text(ContextMenu::tr("Server Settings..."))
        .requiredGlobalPermission(Qn::GlobalAdminPermission)
        .condition(condition::hasFlags(Qn::remote_server, MatchMode::ExactlyOne)
            && !ConditionWrapper(new FakeServerCondition(true))
            && !condition::tourIsRunning()
            && condition::scoped(SceneScope, !condition::isLayoutTourReviewMode()));

    factory(ConnectToCurrentSystem)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Merge to Currently Connected System..."))
        .condition(
            condition::treeNodeType(Qn::ResourceNode)
            && !condition::isSafeMode()
            && ConditionWrapper(new MergeToCurrentSystemCondition())
            && ConditionWrapper(new RequiresOwnerCondition())
        );

    factory()
        .flags(Scene | NoTarget)
        .childFactory(new AspectRatioFactory(manager))
        .text(ContextMenu::tr("Cell Aspect Ratio..."))
        .condition(!ConditionWrapper(new VideoWallReviewModeCondition())
            && ConditionWrapper(new LightModeCondition(Qn::LightModeSingleItem))
            && !condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    factory()
        .flags(Scene | NoTarget)
        .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
        .text(ContextMenu::tr("Cell Spacing..."))
        .condition(ConditionWrapper(new LightModeCondition(Qn::LightModeSingleItem))
            && !condition::isLayoutTourReviewMode()
            && !condition::tourIsRunning());

    // TODO: #GDM Move to childFactory, reduce actions number
    factory.beginSubMenu();
    {
        factory.beginGroup();

        factory(SetCurrentLayoutItemSpacingNoneAction)
            .flags(Scene | NoTarget)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
            .text(ContextMenu::tr("None"))
            .checkable()
            .checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::None));

        factory(SetCurrentLayoutItemSpacingSmallAction)
            .flags(Scene | NoTarget)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
            .text(ContextMenu::tr("Small"))
            .checkable()
            .checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Small));

        factory(SetCurrentLayoutItemSpacingMediumAction)
            .flags(Scene | NoTarget)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
            .text(ContextMenu::tr("Medium"))
            .checkable()
            .checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Medium));

        factory(SetCurrentLayoutItemSpacingLargeAction)
            .flags(Scene | NoTarget)
            .requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission)
            .text(ContextMenu::tr("Large"))
            .checkable()
            .checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Large));
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
        .condition(condition::treeNodeType(Qn::LayoutTourNode))
        .autoRepeat(false);

    factory(ReviewLayoutTourInNewWindowAction)
        .flags(Tree | NoTarget)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Open in New Window"))
        .condition(condition::treeNodeType(Qn::LayoutTourNode))
        .autoRepeat(false);

    factory().flags(Tree).separator().condition(condition::treeNodeType(Qn::LayoutTourNode));

    factory(ToggleLayoutTourModeAction)
        .flags(Scene | Tree | NoTarget | GlobalHotkey)
        .mode(DesktopMode)
        .dynamicText(new LayoutTourTextFactory(manager))
        .shortcut(lit("Alt+T"))
        .checkable()
        .autoRepeat(false)
        .condition(condition::tourIsRunning()
            || (condition::treeNodeType(Qn::LayoutTourNode) && condition::canStartTour()));

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

    factory().flags(Tree).separator().condition(condition::treeNodeType(Qn::LayoutTourNode));

    factory(RemoveLayoutTourAction)
        .flags(Tree | NoTarget | IntentionallyAmbiguous)
        .mode(DesktopMode)
        .text(ContextMenu::tr("Delete"))
        .shortcut(lit("Del"))
        .shortcut(Qt::Key_Backspace, Builder::Mac, true)
        .condition(condition::treeNodeType(Qn::LayoutTourNode));

    factory().flags(Tree).separator().condition(condition::treeNodeType(Qn::LayoutTourNode));

    factory(RenameLayoutTourAction)
        .flags(Tree | NoTarget | IntentionallyAmbiguous)
        .text(ContextMenu::tr("Rename"))
        .shortcut(lit("F2"))
        .condition(condition::treeNodeType(Qn::LayoutTourNode)
            && !condition::isSafeMode())
        .autoRepeat(false);

    factory(SaveLayoutTourAction)
        .flags(NoTarget)
        .mode(DesktopMode);

    factory(RemoveCurrentLayoutTourAction)
        .flags(NoTarget)
        .mode(DesktopMode)
        .condition(condition::isLayoutTourReviewMode())
        .autoRepeat(false);

    factory().flags(Tree).separator().condition(condition::treeNodeType(Qn::LayoutTourNode));

    factory(MakeLayoutTourAction)
        .flags(Tree | SingleTarget | MultiTarget | ResourceTarget)
        .text(ContextMenu::tr("Make Showreel"))
        .condition(condition::hasFlags(Qn::layout, MatchMode::All)
            && !condition::isSafeMode());

    factory(LayoutTourSettingsAction)
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("Settings"))
        .condition(condition::treeNodeType(Qn::LayoutTourNode)
            && !condition::isSafeMode())
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

    factory(DebugIncrementCounterAction)
        .flags(GlobalHotkey | DevMode)
        .shortcut(lit("Ctrl+Alt+Shift++"))
        .text(lit("Increment Debug Counter")); //< DevMode, so untranslatable

    factory(DebugDecrementCounterAction)
        .flags(GlobalHotkey | DevMode)
        .shortcut(lit("Ctrl+Alt+Shift+-"))
        .text(lit("Decrement Debug Counter")); //< DevMode, so untranslatable

    factory(DebugCalibratePtzAction)
        .flags(Scene | SingleTarget | DevMode)
        .text(lit("Calibrate PTZ")); //< DevMode, so untranslatable

    factory(DebugGetPtzPositionAction)
        .flags(Scene | SingleTarget | DevMode)
        .text(lit("Get PTZ Position")); //< DevMode, so untranslatable

    factory(DebugControlPanelAction)
        .flags(GlobalHotkey | DevMode)
        .shortcut(lit("Ctrl+Alt+Shift+D"))
        .text(lit("Debug Control Panel")); //< DevMode, so untranslatable

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
        .shortcut(lit("M"))
        .text(ContextMenu::tr("Toggle Mute"))
        .checkable()
        .condition(new TimelineVisibleCondition());

    factory(JumpToLiveAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("L"))
        .text(ContextMenu::tr("Jump to Live"))
        .checkable()
        .condition(new ArchiveCondition());

    factory(ToggleSyncAction)
        .flags(ScopelessHotkey | HotkeyOnly | Slider | SingleTarget)
        .shortcut(lit("S"))
        .text(ContextMenu::tr("Synchronize Streams"))
        .toggledText(ContextMenu::tr("Disable Stream Synchronization"))
        .condition(ConditionWrapper(new ArchiveCondition())
            && !condition::tourIsRunning());

    factory()
        .flags(Slider | TitleBar | Tree)
        .separator();

    factory(ToggleThumbnailsAction)
        .flags(NoTarget);

    factory(BookmarksModeAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Bookmarks")) //< To be displayed on the button
        .requiredGlobalPermission(Qn::GlobalViewBookmarksPermission)
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

    factory(PinTreeAction)
        .flags(Tree | NoTarget)
        .text(ContextMenu::tr("Pin Tree")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Unpin Tree"))
        .condition(condition::treeNodeType(Qn::RootNode));

    factory(PinCalendarAction)
        .flags(NoTarget)
        .checkable();

    factory(MinimizeDayTimeViewAction)
        .text(ContextMenu::tr("Minimize")) //< To be displayed on button tooltip
        .icon(qnSkin->icon("titlebar/dropdown.png"));

    factory(ToggleTreeAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Tree")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Hide Tree"))
        .condition(condition::treeNodeType(Qn::RootNode));

    factory(ToggleTimelineAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Timeline")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Hide Timeline"));

    factory(ToggleNotificationsAction)
        .flags(NoTarget)
        .text(ContextMenu::tr("Show Notifications")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Hide Notifications"));

    factory(PinNotificationsAction)
        .flags(Notifications | NoTarget)
        .text(ContextMenu::tr("Pin Notifications")) //< To be displayed on button tooltip
        .toggledText(ContextMenu::tr("Unpin Notifications"));

    factory(GoToNextItemAction)
        .flags(NoTarget);

    factory(GoToPreviousItemAction)
        .flags(NoTarget);

    factory(ToggleCurrentItemMaximizationStateAction)
        .flags(NoTarget);

    factory(PtzContinuousMoveAction)
        .flags(NoTarget);

    factory(PtzActivatePresetByIndexAction)
        .flags(NoTarget);
}

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
