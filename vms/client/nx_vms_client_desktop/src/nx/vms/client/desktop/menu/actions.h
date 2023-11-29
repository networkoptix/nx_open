// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <string_view>

#include <QtCore/QMetaType>

#include <nx/reflect/instrument.h>

#include "action_fwd.h"

namespace nx::vms::client::desktop::menu {

Q_NAMESPACE

/**
 * Enum of all menu actions.
 */
enum IDType
{
    /* Actions that are not assigned to any menu. */

    /**
     * Opens login dialog.
     */
    OpenLoginDialogAction,

    /**
     * Connects to the system.
     */
    ConnectAction,

    /**
     * Setup New System.
     * Requires LogonData data in the LogonDataRole.
     */
    SetupFactoryServerAction,

    /**
     * Connects to cloud system.
     */
    ConnectToCloudSystemAction,

    /**
     * Initiates connect to a cloud system with user interaction.
     */
    ConnectToCloudSystemWithUserInteractionAction,

    /**
     * Disconnects from the current Server (universal internal action).
     */
    DisconnectAction,

    /**
     * Disconnects from the current Server (main menu user-only action).
     */
    DisconnectMainMenuAction,

    /**
     * Removes system from system tab bar.
     */
    RemoveSystemFromTabBarAction,

    /**
    * Switches mode to browse resources/show welcome screen
    */
    ResourcesModeAction,

    /**
     * Forcefully disconnects from the current server (if any).
     * Connects to the server using the last used URL set in <tt>QnSettings</tt>.
     */
    ReconnectAction,

    /**
     * Opens licenses preferences tab.
     */
    PreferencesLicensesTabAction,

    /**
     * Opens smtp settings preferences tab.
     */
    PreferencesSmtpTabAction,

    /**
     * Opens notifications settings preferences tab.
     */
    PreferencesNotificationTabAction,

    /**
    * Opens notifications settings preferences tab.
    */
    PreferencesCloudTabAction,

    /**
     * Shows / hides FPS display.
     */
    ShowFpsAction,

    /**
     * Shows / hides debug overlay.
     */
    ShowDebugOverlayAction,

    /**
     * Drops provided resources on the workbench.
     */
    DropResourcesAction,

    /**
     * Handle resources passed to the client.
     */
    ProcessStartupParametersAction,

    /**
     * Moves cameras from one server to another.
     *
     * Parameters.
     *
     * <tt>QnMediaServerResourcePtr MediaServerResourceRole</tt> --- video server to
     * move cameras to.
     */
    MoveCameraAction,

    /**
     * Opens next layout.
     */
    NextLayoutAction,

    /**
     * Opens previous layout.
     */
    PreviousLayoutAction,

    /**
     * Selects all widgets on the scene.
     */
    SelectAllAction,

    /**
     * Notifies action handler about selection changes.
     */
    SelectionChangeAction,

    /**
     * Select newly created item (webpage, showreel, user, videowall) in the resource tree.
     */
    SelectNewItemAction,

    /**
     * Enters "what's this" mode.
     */
    WhatsThisAction,

    /**
     * Cancels Tour Mode if it is started, otherwise works as FullScreenAction.
     */
    EscapeHotkeyAction,

    /**
     * Displays version mismatch dialog, pulling mismatch data from
     * <tt>QnWorkbenchVersionMismatchWatcher</tt>. Displays nothing if there
     * is no mismatches.
     */
    VersionMismatchMessageAction,

    /**
     * Displays beta version warning dialog.
     */
    BetaVersionMessageAction,

    /**
     * Displays warning which describes consequences of updating to the beta version.
     * Should be shown if cloud host is different from the 'prod' or just after the initial
     * beta warning.
     */
    BetaUpgradeWarningAction,

    /**
     * Displays HiDpi screens support warning dialog.
     */
    HiDpiSupportMessageAction,

    /**
     * Asks user to select analytics storage location.
     */
    ConfirmAnalyticsStorageAction,

    /**
     * Opens the provided url in the default browser.
     *
     * Parameters:
     * <tt>QUrl UrlRole</tt> --- target url.
     */
    BrowseUrlAction,

    /**
     * Opens the Business Events Log dialog.
     * Supports cameras list in the resources field as a cameras filter.
     * Parameters:
     * <tt>nx::vms::api::EventType EventTypeRole</tt> --- filter by event type.
     */
    OpenBusinessLogAction,

    /**
     * Opens the Business Rules dialog.
     * Supports cameras list in the resources field as a cameras filter.
     */
    OpenBusinessRulesAction,

    /**
     * Opens the Failover Priority dialog.
     */
    OpenFailoverPriorityAction,

    /**
     * Opens videowall control layouts for all items in the provided QnVideoWallItemIndexList.
     */
    StartVideoWallControlAction,

    /**
     * Sets up desktop camera as a layout for all items in the provided QnVideoWallItemIndexList.
     */
    PushMyScreenToVideowallAction,

    /**
     * Open Video Wall Screen Settings dialog.
     */
    VideoWallScreenSettingsAction,

    /**
     * Saves videowall review layout.
     */
    SaveVideoWallReviewAction,

    /**
     * Saves current videowall review layout.
     */
    SaveCurrentVideoWallReviewAction,

    /**
     * Handles resources drop on the selected videowall item.
     */
    DropOnVideoWallItemAction,

    /**
     * Tries to restart the application as soon as all modal dialogs are closed.
     * Parameters:
     * <tt>QUrl UrlRole</tt>                            --- url the application should connect to.
     *                                                      If not provided, current connection info will be used.
     */
    QueueAppRestartAction,

    /**
     * Offers user to select server in cluster for other servers to synchronize time with
     */
    SelectTimeServerAction,


    /* Right panel actions */

    /**
     * Opens Notifications tab in the right panel.
     */
    NotificationsTabAction,

    /**
     * Opens Cameras & Resources tab in the left panel.
     */
    ResourcesTabAction,

    /**
     * Opens Cameras & Resources tab and sets focus to the search field.
     */
    SearchResourcesAction,

    /**
     * Toggles Motion tab in the right panel.
     */
    MotionTabAction,

    /**
     * Switches the tabs on the right panel between the motion and notification tabs.
     */
    SwitchMotionTabAction,

    /**
     * Opens Bookmarks tab in the right panel.
     */
    BookmarksTabAction,

    /**
     * Opens Events tab in the right panel.
     */
    EventsTabAction,

    /**
     * Opens Objects tab in the right panel.
     */
    ObjectsTabAction,

    /**
     * Checked state indicates that the Right Panel is at Objects tab and in Selected Camera mode.
     */
    ObjectSearchModeAction,

    /* Main menu actions. */

    /**
     * Opens the main menu.
     */
    MainMenuAction,

    /**
     * Opens a new tab (layout).
     * This action is executed every time the last layout on a workbench is closed.
     */
    OpenNewTabAction,

    /**
     * Opens a new window (client instance). It will connect to a current server.
     */
    OpenNewWindowAction,

    /**
     * Opens a new window (client instance) with a welcome screen.
     */
    OpenWelcomeScreenAction,

    /**
     * Saves current layout on appserver.
     */
    SaveCurrentLayoutAction,

    /**
     * Saves current layout under another name.
     */
    SaveCurrentLayoutAsAction,

    /**
     * Saves current layout as a cloud layout for the current user.
     */
    SaveCurrentLayoutAsCloudAction,

    /**
     * Opens a file dialog and adds selected files to the current layout,
     * or opens the selected layouts.
     */
    OpenFileAction,

    /**
     * Opens a file dialog and adds all files from selected folder
     * to the current layout.
     */
    OpenFolderAction,

    /**
     * Starts / stops screen recording.
     */
    ToggleScreenRecordingAction,

    /**
     * Maximizes/restores client's main window.
     */
    MaximizeAction,

    /**
     * Toggles client's fullscreen state.
     */
    FullscreenAction,

    /**
     * Action to be invoked to toggle fullscreen/maximized state.
     * Actual action that will be invoked is platform-dependent.
     */
    EffectiveMaximizeAction,

    /**
     * Just triggers EffectiveMaximizeAction which should be alias to FullscreenAction or MaximizeAction.
     */
    FullscreenMaximizeHotkeyAction,

    /**
     * Goes to fullscreen and slides out all panels.
     */
    FreespaceAction,

    /**
     * Goes central camera/layout to fullscreen and slides out all panels.
     */
    FullscreenResourceAction,

    /**
     * Show timeline on the videowall for some time as a reaction for the some user actions.
     */
    ShowTimeLineOnVideowallAction,

    /**
     * Minimizes client's main window.
     */
    MinimizeAction,

    /**
     * Opens system settings dialog.
     */
    PreferencesGeneralTabAction,

    JoystickSettingsAction,

    /**
     * Opens about dialog.
     */
    AboutAction,

    /**
     * Opens user manual dialog.
     */
    UserManualAction,

    /**
     * Save full session state (i.e. windows configuration for the current system-user pair).
     */
    SaveSessionState,

    /**
     * Restore previously saved session state.
     */
    RestoreSessionState,

    /**
     * Delete previously saved session state.
     */
    DeleteSessionState,

    /**
     * Closes the client. Session state is autosaved.
     */
    ExitAction,

    /**
     * Closes all client windows.
     */
    CloseAllWindowsAction,

    /**
     * Forcibly closes the client asynchronously.
     */
    DelayedForcedExitAction,

    /**
     * Notifies all modules about client closing.
     */
    BeforeExitAction,

    /* Tree Root Nodes actions */

    /**
     * Opens web client in the default browser.
     */
    WebClientAction,

    /**
     * Opens web admin for given server in the default browser.
     */
    WebAdminAction,

    /**
     * Opens business events editing dialog.
     */
    BusinessEventsAction,

    /**
     * Opens bookmarks dialog.
     */
    OpenBookmarksSearchAction,

    /**
     * Opens camera list dialog.
     */
    CameraListAction,

    /**
     * System administration dialog.
     */
    SystemAdministrationAction,

    /**
     * System administration dialog - updates page.
     */
    SystemUpdateAction,

    /**
     * System administration dialog - users page.
     */
    UserManagementAction,

    /**
     * System administration dialog - logs management subpage in advanced settings.
     */
    LogsManagementAction,

    /**
     * Advanced updates settings dialog.
     */
    AdvancedUpdateSettingsAction,

    /**
     * Switches representation of cameras and devices in the Resource Tree and device selection
     * dialogs to the mode with breakdown by parent server.
     */
    ShowServersInTreeAction,

    /**
     * Switches representation of cameras and devices in the Resource Tree and device selection
     * dialogs to the plain mode.
     */
    HideServersInTreeAction,

    /**
     * Toggles representation and behavior of proxied resources in the Resource Tree. By default,
     * all proxied resources are displayed only under the corresponding nodes. When enabled,
     * proxied resources are displayed also under the corresponding server node, it is possible to
     * group them with other server resources and change proxying settings using drag'n'drop.
     */
    ToggleProxiedResources,

    /* Tab bar actions. */

    /**
     * Closes layout.
     */
    CloseLayoutAction,

    /**
     * Closes all layouts but the one provided.
     */
    CloseAllButThisLayoutAction,


    /* Resource actions. */

    /**
     * Opens selected resources in provided layout.
     *
     * Parameters:
     *
     * <tt>QPointF ItemPositionRole</tt> --- drop position, in grid coordinates.
     * If not provided, Items will be dropped at the center of the layout.
     * <tt>QnLayoutResourcePtr LayoutResourceRole</tt> --- layout to drop at.
     */
    OpenInLayoutAction,

    /**
     * Opens selected resources in current layout.
     *
     * Parameters:
     *
     * <tt>QPointF ItemPositionRole</tt> --- drop position, in grid coordinates.
     * If not provided, Items will be dropped at the center of the layout.
     */
    OpenInCurrentLayoutAction,

    /**
     * Opens selected resources in a new tab.
     */
    OpenInNewTabAction,

    /**
     * Opens Intercom Layout when clicking on a notification tile.
     * Closes the notification on all clients.
     */
    OpenIntercomLayoutAction,

    /**
     * Opens selected resources in the Alarm Layout.
     */
    OpenInAlarmLayoutAction,

    /**
     * Opens selected resources in a new window.
     */
    OpenInNewWindowAction,

    /**
     * Opens given videowall in review mode.
     */
    OpenVideoWallReviewAction,

    /**
     * Menu containing all layouts belonging to the current user.
     */
    OpenCurrentUserLayoutMenu,

    /**
     * Opens current layout in a new window.
     */
    OpenCurrentLayoutInNewWindowAction,

    /**
     * Saves selected layout.
     */
    SaveLayoutAction,

    /**
     * Saves selected local layout.
     */
    SaveLocalLayoutAction,

    /**
     * Saves selected layout as a cloud layout for the current user.
     */
    SaveLayoutAsCloudAction,

    /**
    * Forgets password for encrypted layout.
    */
    ForgetLayoutPasswordAction,

    /**
     * Saves provided layout under another name.
     */
    SaveLayoutAsAction,

    /**
     * Converts selected layout to shared.
     */
    ConvertLayoutToSharedAction,

    /**
     * Performs a fit in view operation.
     */
    FitInViewAction,

    /**
     * Maximizes item.
     */
    MaximizeItemAction,

    /**
     * Unmaximizes item.
     */
    UnmaximizeItemAction,

    /**
     * Shows motion on an item and turns on smart search mode.
     */
    StartSmartSearchAction,

    /**
     * Hides motion on an item and turns off smart search mode.
     */
    StopSmartSearchAction,

    /**
     * Clears motion selection on a widget.
     */
    ClearMotionSelectionAction,

    /**
     * Toggles item's smart search mode.
     */
    ToggleSmartSearchAction,

    /**
     * Check file signature (for local files only)
     */
    CheckFileSignatureAction,

    /**
     * Takes screenshot of an item.
     *
     * Parameters:
     * <tt>QString FileNameRole</tt> --- name for the screenshot. If not provided,
     * a file selection dialog will pop up.
     */
    TakeScreenshotAction,

    /**
     * Change video contrast
     *
     */
    AdjustVideoAction,

    /**
     * Opens user settings dialog.
     */
    UserSettingsAction,

    /**
     * Opens User Groups dialog.
     */
    UserGroupsAction,

    /**
     * Opens camera settings dialog.
     */
    CameraSettingsAction,

    /**
     * Opens picture settings dialog.
     */
    MediaFileSettingsAction,

    /**
     * Opens videowall settings dialog.
     */
    VideowallSettingsAction,

    /**
     * Opens analytics engine settings dialog.
     */
    AnalyticsEngineSettingsAction,

    /**
     * Opens event log dialog with filter for current cameras issues
     */
    CameraIssuesAction,

    /**
     * Opens business rules dialog with filter for current cameras rules
     */
    CameraBusinessRulesAction,

    /**
     * Opens camera diagnostics dialog that checks for problems with
     * selected camera.
     */
    CameraDiagnosticsAction,

    /**
     * Opens current layout settings dialog.
     */
    CurrentLayoutSettingsAction,

    /**
     * Opens layout settings dialog.
     */
    LayoutSettingsAction,

    /**
     * Opens server settings dialog.
     */
    ServerSettingsAction,

    /**
     * Opens a console with ping process for the selected resource.
     */
    PingAction,

    /**
     * Opens server logs in the default web browser.
     */
    ServerLogsAction,

    /**
     * Opens event log dialog with filter for current servers issues.
     */
    ServerIssuesAction,

    /**
     * Opens manual device addition dialog.
     */
    AddDeviceManuallyAction,

    /**
     * Opens manual device addition dialog from main menu.
     */
    MainMenuAddDeviceManuallyAction,

    /**
     * Opens camera list by server
     */
    CameraListByServerAction,

    /**
     * Opens a folder that contains the file resource.
     */
    OpenInFolderAction,

    /**
     * Creates a zoom window for the given item.
     */
    CreateZoomWindowAction,

    /**
     * Rotates item to specified orientation.
     */
    RotateToAction,

    /**
     * Displays info widget on all selected items
     */
    ShowInfoAction,

    /**
     * Hides info widget on all selected items
     */
    HideInfoAction,

    /**
     * Hides info widget if it is visible on all items, otherwise displays on all items
     */
    ToggleInfoAction,

    /**
     * Changes RADASS mode for the given layout or item. Mode provided with ResolutionModeRole.
     */
    RadassAction,

    /**
     * For the camera resource list with the same Composite Group ID with dimension less then
     * stated limit, appends new unique Group ID to the top if initial Composite Group ID for each
     * camera resource.
     *
     * As observed result, new resource group containing camera resource items as direct children
     * appears in the Resource Tree.
     */
    CreateNewCustomGroupAction,

    /**
     * Explicitly assigns given Composite Group ID to the all given camera resources.
     * Equals to making all given camera resources items direct children of given group, not
     * necessarily existing.
     */
    AssignCustomGroupIdAction,

    /**
     * Add documentation,
     */
    MoveToCustomGroupAction,

    /**
     * For the camera resource list with Composite Group ID equal up to the given order,
     * substitutes Group ID with given order with another one for each camera resource.
     *
     * As observed result, certain single resource group is renamed.
     */
    RenameCustomGroupAction,

    /**
     * For the camera resource list with Composite Group ID equal up to the given order,
     * Group ID for the given order is removed for each camera resource. Dimension of
     * Composite Group ID for each mentioned camera resource decrements by 1.
     *
     * As observed result, certain resource group is removed, all child resources and groups became
     * on the level of the removed group.
     */
    RemoveCustomGroupAction,

    /**
     * Triggered right after successful new group creation.
     */
    NewCustomGroupCreatedEvent,

    /**
     * Triggered right after successful new group creation.
     */
    CustomGroupIdAssignedEvent,

    /**
     * Triggered right after group reposition.
     */
    MovedToCustomGroupEvent,

    /**
     * Triggered right after group renaming.
     */
    CustomGroupRenamedEvent,

    /**
     * Triggered right after successful group removal.
     */
    CustomGroupRemovedEvent,

    /**
     * Connect incompatible server to current system
     */
    ConnectToCurrentSystem,

    /**
     * Merge the other system with the current system
     */
    MergeSystems,

    /* PTZ Actions */

    /**
     * Opens preset name editing dialog and saves current position as a new PTZ preset.
     */
    PtzSavePresetAction,

    /**
     * Menu containing all PTZ presets.
     */
    PtzGoToPresetMenu,

    /**
     * Moves camera to the given PTZ preset.
     *
     * Parameters:
     * <tt>QString PtzObjectIdRole</tt> --- id of the PTZ preset.
     */
    PtzActivatePresetAction,

    /**
     * Starts given PTZ tour.
     *
     * Parameters:
     * <tt>QString PtzObjectIdRole</tt> --- id of the PTZ tour.
     */
    PtzActivateTourAction,

    /**
     * Activates given PTZ object.
     *
     * Parameters:
     * <tt>QString PtzObjectIdRole</tt> --- id of the PTZ preset/tour.
     */
    PtzActivateObjectAction,

    /**
     * Activates PTZ object that is assigned to the given hotkey.
     *
     * Parameters:
     * <tt>int PtzPresetIndexRole</tt> --- id of the hotkey (0..9).
     */
    PtzActivateByHotkeyAction,

    /**
     * Opens PTZ tours management dialog.
     */
    PtzManageAction,

    /**
     * Performs continuous move with given speed.
     */
    PtzContinuousMoveAction,

    /**
     * Activates preset by index
     */
    PtzActivatePresetByIndexAction,

    /**
     *
     */
    PtzFocusInAction,

    /**
     *
     */
    PtzFocusOutAction,

    /**
     *
     */
    PtzFocusAutoAction,

    /* Layout actions. */

    /**
     * Sets the current picture as a layout background.
     */
    SetAsBackgroundAction,

    /**
     * Deletes the file from disk.
     */
    DeleteFromDiskAction,

    /**
     * Removes items from layouts.
     */
    RemoveLayoutItemAction,
    RemoveLayoutItemFromSceneAction,

    /**
     * Replaces layout item seamlessly with another one.
     */
    ReplaceLayoutItemAction,

    /**
     * Removes a resource from Server.
     */
    RemoveFromServerAction,

    /**
     * Changes resource name.
     *
     * Parameters:
     *
     * <tt>QString ResourceNameRole</tt> --- new name for the resource. If not
     * supplied, name dialog will pop up.
     */
    RenameResourceAction,

    /**
    * Changes resource name.
    *
    * Parameters:
    *
    * <tt>QString ResourceNameRole</tt> --- new name for the entity. If not
    * supplied, name dialog will pop up.
    */
    RenameVideowallEntityAction,

    /**
     * Opens a user creation dialog.
     */
    NewUserAction,

    /**
     * Opens an integration creation dialog.
     */
    NewIntegrationAction,

    /**
     * Opens a webpage creation dialog.
     */
    NewWebPageAction,

    /**
     * Opens a videowall creation dialog.
     */
    NewVideoWallAction,

    /**
     * Attaches current client window to the selected videowall.
     */
    AttachToVideoWallAction,

    /**
     * Detaches selected layouts from the videowall.
     */
    ClearVideoWallScreen,

    /**
     * Deletes selected videowall items.
     */
    DeleteVideoWallItemAction,

    /**
     * Start another client instance in the videowall master mode.
     */
    StartVideoWallAction,

    /**
     * Stop all client instances running this videowall.
     */
    StopVideoWallAction,

    /**
     * Display identification messages on the videowall screens.
     */
    IdentifyVideoWallAction,

    /**
     * Save current videowall matrix.
     */
    SaveVideowallMatrixAction,

    /**
     * Load previously saved videowall matrix.
     */
    LoadVideowallMatrixAction,

    /**
     * Delete saved videowall matrix.
     */
    DeleteVideowallMatrixAction,

    /**
     * Open target videowall item after user logged in.
     */
    DelayedOpenVideoWallItemAction,

    /**
     * Opens a layout creation dialog.
     */
    NewUserLayoutAction,

    /**
     * Sets spacing of current layout's cells to None.
     */
    SetCurrentLayoutItemSpacingNoneAction,

    /**
     * Sets spacing of current layout's cells to Small.
     */
    SetCurrentLayoutItemSpacingSmallAction,

    /**
     * Sets spacing of current layout's cells to Medium.
     */
    SetCurrentLayoutItemSpacingMediumAction,

    /**
     * Sets spacing of current layout's cells to Large.
     */
    SetCurrentLayoutItemSpacingLargeAction,

    /**
     * Toggles Showreel mode.
     */
    ToggleShowreelModeAction,

    NewShowreelAction,
    MakeShowreelAction,
    RenameShowreelAction,
    SaveShowreelAction,
    ReviewShowreelAction,
    ReviewShowreelInNewWindowAction,
    RemoveShowreelAction,

    StartCurrentShowreelAction,
    SaveCurrentShowreelAction,
    RemoveCurrentShowreelAction,

    /** Suspend Showreel cycling. */
    SuspendCurrentShowreelAction,

    /** Resume Showreel cycling. */
    ResumeCurrentShowreelAction,

    ShowreelSettingsAction,
    CurrentShowreelSettingsAction,

    AddVirtualCameraAction,
    MainMenuAddVirtualCameraAction,
    UploadVirtualCameraFileAction,
    UploadVirtualCameraFolderAction,
    CancelVirtualCameraUploadsAction,

    /* Timeline actions. */

    /**
     * Starts selection.
     */
    StartTimeSelectionAction,

    /**
     * Ends selection.
     */
    EndTimeSelectionAction,

    /**
     * Clears selection.
     */
    ClearTimeSelectionAction,

    /**
     * Scale timeline to match the selection.
     */
    ZoomToTimeSelectionAction,

    /**
     * Exports standalone exe client.
     */
    ExportStandaloneClientAction,

    /**
     * Exports selected range.
     */
    ExportVideoAction,

    /**
     * Exports selected bookmark.
     */
    ExportBookmarkAction,

    /**
     * Exports several selected bookmarks into a single layout.
     */
    ExportBookmarksAction,

    /**
     * Copy selected bookmark text.
     */
    CopyBookmarkTextAction,

    /**
     * Copy several selected bookmarks text.
     */
    CopyBookmarksTextAction,

    /**
     * Bookmark selected range.
     */
    AddCameraBookmarkAction,

    /**
     * Edit selected bookmark.
     */
    EditCameraBookmarkAction,

    /**
     * Remove selected bookmark.
     */
    RemoveBookmarkAction,

    /**
     * Batch bookmarks deleting.
     */
    RemoveBookmarksAction,

    AcknowledgeEventAction,

    /**
     * Opens new layout for Quick Search.
     *
     * Parameters:
     *
     * <tt>QnTimePeriod TimePeriodRole</tt> --- time period for quick search.
     */
    ThumbnailsSearchAction,

    /**
     * Shows/hides thumbnails.
     */
    ToggleThumbnailsAction,

    /**
     * Shows/hides bookmarks.
     */
    BookmarksModeAction,

    /**
     * Shows/hides calendar.
     */
    ToggleCalendarAction,

    /**
     * Shows/hides title bar.
     */
    ToggleTitleBarAction,

    /**
     * Shows/hides the resource tree.
     */
    ToggleTreeAction,

    /**
     * Shows/hides the left panel.
     */
    ToggleLeftPanelAction = ToggleTreeAction,

    /**
     * Shows/hides timeline.
     */
    ToggleTimelineAction,

    /**
     * Pins timeline.
     */
    PinTimelineAction,

    /**
    * Shows/hides notification panel.
    */
    ToggleNotificationsAction,

    /* Playback actions. */
    PlayPauseAction,
    PreviousFrameAction,
    NextFrameAction,
    JumpToStartAction,
    JumpToEndAction,
    VolumeUpAction,
    VolumeDownAction,
    ToggleMuteAction,
    JumpToLiveAction,
    ToggleSyncAction,
    JumpToTimeAction,

    FastForwardAction,
    RewindAction,

    /* Debug actions. */

    /**
     * Increments debug counter.
     */
    DebugIncrementCounterAction,

    /**
     * Decrements debug counter.
     */
    DebugDecrementCounterAction,

    /**
     * Toggles element hightlighting for UI elements under cursor.
     */
    DebugToggleElementHighlight,

    /**
     * Toggles `securityForPowerUsers` system flag; available only for Administrators.
     */
    DebugToggleSecurityForPowerUsersAction,

    /**
     * Generates PTZ calibration screenshots.
     */
    DebugCalibratePtzAction,

    DebugGetPtzPositionAction,

    /**
     * Opens up debug control panel.
     */
    DebugControlPanelAction,

    /**
     * Opens the Audit Log dialog.
     */
    OpenAuditLogAction,

    /**
     * Opens Login to Cloud dialog.
     */
    LoginToCloud,

    /**
     * Logs out the user from cloud clearing the stored password.
     */
    LogoutFromCloud,

    /**
     * Opens cloud portal in the browser at account security page.
     */
    OpenCloudAccountSecurityUrl,

    /**
    * Opens cloud portal in the browser at register page.
    */
    OpenCloudRegisterUrl,

    /**
     * Opens cloud portal in the browser.
     */
    OpenCloudMainUrl,

    /**
     * Opens cloud system view in the browser.
     */
    OpenCloudViewSystemUrl,

    /**
     * Opens cloud account management page in the browser.
     */
    OpenCloudManagementUrl,

    /**
     * Go to the next item on layout.
     */
    GoToNextItemAction,

    /**
     * Go to the previous item on layout
     */
    GoToPreviousItemAction,

    /**
     * Go to the next row on layout.
     */
    GoToNextRowAction,

    /**
     * Go to the previous row on layout
     */
    GoToPreviousRowAction,

    /**
     * Go to either specified item on current layout (argument with Qn::ItemUuidRole)
     * or to the first item of specified resource on current layout (resource parameter)
     */
    GoToLayoutItemAction,

    /**
     * Toggles rised state of the current layout item.
     */
    RaiseCurrentItemAction,

    /**
     * Maximizes/Unmaximizes current item
     **/
    ToggleCurrentItemMaximizationStateAction,

    /**
     * Opens Integration settings dialog.
     */
    IntegrationSettingsAction,

    /**
     * Opens Web Page settings dialog.
     */
    WebPageSettingsAction,

    ChangeDefaultCameraPasswordAction,

    /**
     * Opens dialog to copy recording parameters & schedule of specified camera.
     */
    CopyRecordingScheduleAction,

    /**
     * Tries to start current Resource Tree item editing.
     */
    EditResourceInTreeAction,

    // TODO: #dklychkov Remove when the new scene engine becomes default.
    OpenNewSceneAction,

    /**
     * Opens new "VMS Rules" dialog which should replace the old one at certain point.
     * @note Developer Mode and presence of new Event Rules Engine are required.
     * @note Will replace 'OpenBusinessRulesAction'.
     */
    OpenVmsRulesDialogAction,

    /**
     * Control in which mode analytics objects must be displayed for the selected cameras.
     */
    AnalyticsObjectsVisualizationModeAction,

    /**
     * Connect to chosen server and select it as preferred.
     */
    SelectCurrentServerAction,

    /**
     * Notifies all modules about finished video export.
     */
    ExportFinishedEvent,

    /**
     * Notifies all modules about received initial resources after the server has been connected.
     */
    InitialResourcesReceivedEvent,

    /**
     * Bookmarks info is fetched from from the server.
     */
    BookmarksPrefetchEvent,

    /**
     * Widget added on layout.
     */
    WidgetAddedEvent,

    /**
     * Watermark check result.
     */
    WatermarkCheckedEvent,

    /**
     * Opens a dialog for adding Proxied Integration to the server.
     */
    AddProxiedIntegrationAction,

    /**
     * Opens a dialog for adding Proxied Web Page to the server.
     */
    AddProxiedWebPageAction,

    /**
     * Opens a dialog for analytics object search.
     */
    OpenAdvancedSearchDialog,

    /**
     * Setup chunks filter.
     *
     * Parameters:
     * <tt>nx::vms::api::StorageLocation StorageLocationRole</tt>
     */
    ChunksFilterAction,

    /**
     * Opens Camera Replacement dialog.
     */
    ReplaceCameraAction,

    /**
     * Discards camera replacement.
     */
    UndoReplaceCameraAction,

    /**
     * Opens Lookup Lists dialog.
     */
    OpenLookupListsDialogAction,

    /**
     * Open Import From Devices dialog.
     */
    OpenImportFromDevices,

    /** Open Event Log dialog. Should replace OpenBusinessLogAction. */
    OpenEventLogAction,

    ActionCount,

    NoAction = -1
};

Q_ENUM_NS(IDType)

void initialize(Manager* manager, Action* root);

std::string toString(IDType id);
bool fromString(const std::string_view& str, IDType* id);

NX_REFLECTION_TAG_TYPE(IDType, useStringConversionForSerialization)

} // namespace nx::vms::client::desktop::menu
