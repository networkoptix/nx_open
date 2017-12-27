#pragma once

#include <QMetaType>

#include <nx/client/desktop/ui/actions/action_fwd.h>

#include <client/client_globals.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(action, IDType, )

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
     * Connects to server.
     */
    ConnectAction,

    /**
    * Connects to cloud system.
    */
    ConnectToCloudSystemAction,

    /**
     * Disconnects from server.
     */
    DisconnectAction,

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
     * Drops provided serialized resources on the current layout after
     * connection to Server was established.
     *
     * Parameters:
     *
     * <tt>QByteArray SerializedDataRole</tt> --- a serialized
     * QnMimeData representation of a set of resources.
     */
    DelayedDropResourcesAction,

    /**
     * Instantly drops provided serialized resources on the current layout.
     *
     * Parameters:
     *
     * <tt>QByteArray SerializedDataRole</tt> --- a serialized
     * QnMimeData representation of a set of resources.
     */
    InstantDropResourcesAction,

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
     * Notifies action handler about selectin changes.
     */
    SelectionChangeAction,

    /**
     * Select newly created item (webpage, layout tour, user, videowall) in the resource tree. Open
     * on the scene if possible.
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
     * Displays HiDpi screens support warning dialog.
     */
    HiDpiSupportMessageAction,

    /**
     * Displays dialog asking about statistics reporting.
     */
    AllowStatisticsReportMessageAction,

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
     * <tt>nx::vms::event::EventType EventTypeRole</tt> --- filter by event type.
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
     * Opens the Backup Cameras dialog.
     */
    OpenBackupCamerasAction,

    /**
     * Opens videowall control layouts for all items in the provided QnVideoWallItemIndexList.
     */
    StartVideoWallControlAction,

    /**
     * Sets up desktop camera as a layout for all items in the provided QnVideoWallItemIndexList.
     */
    PushMyScreenToVideowallAction,

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
     * Opens a new window (client instance).
     */
    OpenNewWindowAction,

    /**
     * Saves current layout on appserver.
     */
    SaveCurrentLayoutAction,

    /**
     * Saves current layout under another name.
     */
    SaveCurrentLayoutAsAction,

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
     * Minimizes client's main window.
     */
    MinimizeAction,

    /**
     * Opens system settings dialog.
     */
    PreferencesGeneralTabAction,


    /**
     * Opens about dialog.
     */
    AboutAction,

    /**
     * Closes the client.
     */
    ExitAction,

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
     * Opens given camera in analytics mode.
     */
    StartAnalyticsAction,

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
    SaveLocalLayoutAsAction,

    /**
     * Saves selected layout under another name.
     *
     * Parameters:
     * <tt>QnUserResourcePtr UserResourceRole</tt> --- user to assign layout to.
     * <tt>QString ResourceNameRole</tt> --- name for the new layout.
     */
    SaveLayoutAsAction,

    /**
     * Shares selected layout with another user.
     *
     * Parameters:
     * <tt>QnUserResourcePtr UserResourceRole</tt> --- user to share layout with.
     */
    ShareLayoutAction,

    /**
     * Stops sharing selected layout from the selected user.
     *
     * Parameters:
     * <tt>QnUserResourcePtr UserResourceRole</tt> --- user to stop sharing layout with.
     */
    StopSharingLayoutAction,

    /**
     * Saves selected layout under another name in current user's layouts list.
     *
     * Parameters:
     * <tt>QString ResourceNameRole</tt> --- name for the new layout.
     */
    SaveLayoutForCurrentUserAsAction,

    /**
    * Shares selected camera with another user.
    *
    * Parameters:
    * <tt>QnUserResourcePtr UserResourceRole</tt> --- user or role to share camera with.
    */
    ShareCameraAction,

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
     * Opens user roles dialog.
     */
    UserRolesAction,

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
     * Opens event log dialog with filter for current camera(s) issues
     */
    CameraIssuesAction,

    /**
     * Opens business rules dialog with filter for current camera(s) rules
     */
    CameraBusinessRulesAction,

    /**
     * Opens camera diagnostics dialog that checks for problems with
     * selected camera.
     */
    CameraDiagnosticsAction,

    ConvertCameraToEntropix,

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
     * Opens event log dialog with filter for current server(s) issues.
     */
    ServerIssuesAction,

    /**
     * Opens manual camera addition dialog.
     */
    ServerAddCameraManuallyAction,

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
     * Rotates item to normal orientation
     */
    Rotate0Action,

    /**
     * Rotates item to 90 degrees clockwise
     */
    Rotate90Action,

    /**
     * Rotates item to 180 degrees clockwise
     */
    Rotate180Action,

    /**
     * Rotates item to 270 degrees clockwise
     */
    Rotate270Action,

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
     * Removes item(s) from layout(s).
     */
    RemoveLayoutItemAction,
    RemoveLayoutItemFromSceneAction,

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
     * Detaches selected layout(s) from the videowall.
     */
    ClearVideoWallScreen,

    /**
     * Deletes selected videowall item(s).
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
     * Toggles panic recording.
     */
    TogglePanicModeAction,

    /**
     * Toggles layout tour mode.
     */
    ToggleLayoutTourModeAction,

    NewLayoutTourAction,
    MakeLayoutTourAction,
    RenameLayoutTourAction,
    SaveLayoutTourAction,
    ReviewLayoutTourAction,
    ReviewLayoutTourInNewWindowAction,
    RemoveLayoutTourAction,

    StartCurrentLayoutTourAction,
    SaveCurrentLayoutTourAction,
    RemoveCurrentLayoutTourAction,

    LayoutTourSettingsAction,
    CurrentLayoutTourSettingsAction,

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
     * Exports selected range.
     */
    ExportVideoAction,

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
    RemoveCameraBookmarkAction,

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
     * Shows/hides tree.
     */
    ToggleTreeAction,

    /**
     * Pins/unpins tree.
     */
    PinTreeAction,

    /**
     * Pins/unpins calendar and day/time view
     */
    PinCalendarAction,

    /**
     * Minimizes day/time view
     */
    MinimizeDayTimeViewAction,

    /**
     * Shows/hides timeline.
     */
    ToggleTimelineAction,

    ToggleNotificationsAction,

    PinNotificationsAction,

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

    /** Hide cloud promo */
    HideCloudPromoAction,

    /**
     * Go to the next item on layout.
     */
    GoToNextItemAction,

    /**
     * Go to the previous item on layout
     */
    GoToPreviousItemAction,

    /**
     * Maximizes/Unmaximizes current item
     **/
    ToggleCurrentItemMaximizationStateAction,

    /**
    * Opens Web Page settings dialog.
    */
    WebPageSettingsAction,

    ChangeDefaultCameraPasswordAction,

    /** Start searhing for local files */
    UpdateLocalFilesAction,

    // TODO: #dklychkov Remove when the new scene engine becomes default.
    OpenNewSceneAction,

    ActionCount,

    NoAction = -1
};

void initialize(Manager* manager, Action* root);

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::client::desktop::ui::action::IDType, (lexical)(metatype))



