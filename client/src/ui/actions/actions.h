#ifndef QN_ACTIONS_H
#define QN_ACTIONS_H

#include <QMetaType>

#include "action_fwd.h"

#include <client/client_globals.h>

namespace Qn {
    /**
     * Enum of all menu actions.
     */
    enum ActionId {
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
         * Disconnects from server.
         */
        DisconnectAction,

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
         * Drops provided resources on the workbench, opening them in a new
         * layout if necessary.
         */
        DropResourcesIntoNewLayoutAction,

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
         * Enters "what's this" mode.
         */
        WhatsThisAction,

        /**
         * Clears application cache folders (layout backgrounds, sounds)
         */
        ClearCacheAction,

        /**
         * Cancels Tour Mode if it is started, otherwise works as FullScreenAction.
         */
        EscapeHotkeyAction,

        /**
         * Displays message box with the text provided.
         *
         * Parameters:
         * <tt>QString TitleRole</tt> --- title for the messagebox.
         * <tt>QString TextRole</tt> --- displayed text. If not provided, title will be used.
         */
        MessageBoxAction,

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
         * <tt>QnBusiness::EventType EventTypeRole</tt> --- filter by event type.
         */
        OpenBusinessLogAction,

        /**
         * Opens the Business Rules dialog.
         * Supports cameras list in the resources field as a cameras filter.
         */
        OpenBusinessRulesAction,

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
         * <tt>QnSoftwareVersion  SoftwareVersionRole</tt>  --- application version that should be started.
         *                                                      If not provided, current version will be used.
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
         * Opens a file dialog and opens selected layouts.
         */
        OpenLayoutAction,

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
         * Open Showcase page in default browser
         */
        ShowcaseAction,

        /**
         * Closes the client.
         */
        ExitAction,

        /** 
         * Closes the client asynchronously.
         */
        ExitActionDelayed,

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
         * Opens business events editing dialog.
         */
        BusinessEventsAction,

        /**
         * Opens business events log dialog.
         */
        BusinessEventsLogAction,

        /**
         * Opens camera list dialog.
         */
        CameraListAction,

        /**
         * System administration dialog.
         */
        SystemAdministrationAction,

        //ShowMediaServerLogs,


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
         * Opens selected resources in a new layout.
         */
        OpenInNewLayoutAction,

        /**
         * Opens selected resources in a new window.
         */
        OpenInNewWindowAction,

        /**
         * Opens a single layout.
         */
        OpenSingleLayoutAction,

        /**
         * Opens multiple layouts.
         */
        OpenMultipleLayoutsAction,

        /**
         * Opens given layouts.
         */
        OpenAnyNumberOfLayoutsAction,

        /**
         * Opens given videowalls in review mode.
         */
        OpenVideoWallsReviewAction,

        /**
         * Menu containing all layouts belonging to the current user.
         */
        OpenCurrentUserLayoutMenu,

        /**
         * Opens selected layouts in a new window.
         */
        OpenLayoutsInNewWindowAction,

        /**
         * Opens current layout in a new window.
         */
        OpenCurrentLayoutInNewWindowAction,

        /**
         * Saves selected layout.
         */
        SaveLayoutAction,

        /**
         * Saves selected layout under another name.
         *
         * Parameters:
         * <tt>QnUserResourcePtr UserResourceRole</tt> --- user to assign layout to.
         * <tt>QString ResourceNameRole</tt> --- name for the new layout.
         */
        SaveLayoutAsAction,

        /**
         * Saves selected layout under another name in current user's layouts list.
         *
         * Parameters:
         * <tt>QString ResourceNameRole</tt> --- name for the new layout.
         */
        SaveLayoutForCurrentUserAsAction,

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
         * Opens camera settings dialog.
         */
        CameraSettingsAction,

        /**
         * Opens picture settings dialog.
         */
        PictureSettingsAction,

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

        /**
         * Opens current layout settings dialog.
         */
        CurrentLayoutSettingsAction,

        /**
         * Opens layout settings dialog.
         */
        LayoutSettingsAction,

        /**
         * Opens provided resources in an existing camera settings dialog.
         */
        OpenInCameraSettingsDialogAction,

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
         * Changes RADASS mode to auto.
         */
        RadassAutoAction,

        /**
         * Changes RADASS mode to high resolution.
         */
        RadassHighAction,

        /**
         * Changes RADASS mode to low resolution.
         */
        RadassLowAction,

        /**
         * Toggles next RADASS state.
         */
        ToggleRadassAction,

        /**
         * Connect incompatible server to current system
         */
        ConnectToCurrentSystem,

        /**
         * Joing the other system to the current system
         */
        JoinOtherSystem,

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
         * Starts fisheye calibration for the given widget.
         */
        PtzCalibrateFisheyeAction,


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
        RenameAction,

        /**
         * Opens a user creation dialog.
         */
        NewUserAction,

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
        DetachFromVideoWallAction,

        /**
         * Update selected videowall item(s) with the current layout.
         */
        ResetVideoWallLayoutAction,

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
         * Sets aspect ratio of current layout's cells to 16x9.
         */
        SetCurrentLayoutAspectRatio16x9Action,

        /**
         * Sets aspect ratio of current layout's cells to 4x3.
         */
        SetCurrentLayoutAspectRatio4x3Action,

        /**
         * Sets spacing of current layout's cells to 0%.
         */
        SetCurrentLayoutItemSpacing0Action,

        /**
         * Sets spacing of current layout's cells to 10%.
         */
        SetCurrentLayoutItemSpacing10Action,

        /**
         * Sets spacing of current layout's cells to 20%.
         */
        SetCurrentLayoutItemSpacing20Action,

        /**
         * Sets spacing of current layout's cells to 30%.
         */
        SetCurrentLayoutItemSpacing30Action,

        /**
         * Toggles panic recording.
         */
        TogglePanicModeAction,

        /**
         * Toggles tour mode.
         */
        ToggleTourModeAction,

        /* Slider actions. */

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
        ExportTimeSelectionAction,

        /**
         * Exports whole layout.
         */
        ExportLayoutAction,

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
         * Shows/hides slider.
         */
        ToggleSliderAction,

        /** 
         * Shows/hides bookmarks search panel.
         */
        ToggleBookmarksSearchAction,

        PinNotificationsAction,

        /* Playback actions. */
        PlayPauseAction,
        SpeedDownAction,
        SpeedUpAction,
        PreviousFrameAction,
        NextFrameAction,
        JumpToStartAction,
        JumpToEndAction,
        VolumeUpAction,
        VolumeDownAction,
        ToggleMuteAction,
        JumpToLiveAction,
        ToggleSyncAction,

        /**
         * Toggle the background animation.
         */
        ToggleBackgroundAnimationAction,

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
         * Shows resource pool.
         */
        DebugShowResourcePoolAction,

        /**
         * Generates PTZ calibration screenshots.
         */
        DebugCalibratePtzAction,

        DebugGetPtzPositionAction,

        /**
         * Opens up debug control panel.
         */
        DebugControlPanelAction,


        ActionCount,

        NoAction = -1
    };

    /**
     * Scope of an action.
     *
     * Scope defines the menus in which an action can appear, and target
     * for getting the action's parameters in case it was triggered with a
     * hotkey.
     */
    enum ActionScope {
        InvalidScope            = 0x00000000,
        MainScope               = 0x00000001,           /**< Action appears in main menu. */
        SceneScope              = 0x00000002,           /**< Action appears in scene context menu and its parameters are taken from the scene. */
        TreeScope               = 0x00000004,           /**< Action appears in tree context menu. */
        SliderScope             = 0x00000008,           /**< Action appears in slider context menu. */
        TitleBarScope           = 0x00000010,           /**< Action appears title bar context menu. */
        NotificationsScope      = 0x00000020,
        ScopeMask               = 0x000000FF
    };
    Q_DECLARE_FLAGS(ActionScopes, ActionScope);

    /**
     * Type of an action parameter.
     *
     * Note that some of these types are convertible to other types.
     */
    enum ActionParameterType {
        ResourceType            = 0x00000100,           /**< Resource, <tt>QnResourcePtr</tt>. */
        LayoutItemType          = 0x00000200,           /**< Layout item, <tt>QnLayoutItemIndex</tt>. Convertible to resource. */
        WidgetType              = 0x00000400,           /**< Resource widget, <tt>QnResourceWidget *</tt>. Convertible to layout item and resource. */
        LayoutType              = 0x00000800,           /**< Workbench layout, <tt>QnWorkbenchLayout *</tt>. Convertible to resource. */
        VideoWallItemType       = 0x00001000,           /**< Videowall item, <tt>QnVideoWallItemIndex</tt>. Convertible to resource. */
        VideoWallMatrixType     = 0x00002000,           /**< Videowall matrix, <tt>QnVideoWallMatrixIndex</tt>. */
        OtherType               = 0x00004000,           /**< Some other type. */
        TargetTypeMask          = 0x0000FF00
    };
    Q_DECLARE_FLAGS(ActionParameterTypes, ActionParameterType)

    enum ActionFlag {
        /** Action can be applied when there are no targets. */
        NoTarget                = 0x00010000,

        /** Action can be applied to a single target. */
        SingleTarget            = 0x00020000,

        /** Action can be applied to multiple targets. */
        MultiTarget             = 0x00040000,

        /** Action accepts resources as target. */
        ResourceTarget          = ResourceType,

        /** Action accepts layout items as target. */
        LayoutItemTarget        = LayoutItemType,

        /** Action accepts resource widgets as target. */
        WidgetTarget            = WidgetType,

        /** Action accepts workbench layouts as target. */
        LayoutTarget            = LayoutType,

        /** Action accepts videowall items as target. */
        VideoWallItemTarget     = VideoWallItemType,

        /** Action accepts videowall matrices as target. */
        VideoWallMatrixTarget   = VideoWallMatrixType,

        /** Action has a hotkey that is intentionally ambiguous.
         * It is up to the user to ensure that proper action conditions make it
         * impossible for several actions to be triggered by this hotkey. */
        IntentionallyAmbiguous  = 0x00100000,

        /** When the action is activated via hotkey, its scope should not be compared to the current one.
         * Action can be executed from any scope, and its target will be taken from its scope. */
        ScopelessHotkey         = 0x00200000,

        /** When the action is activated via hotkey, it will be executed with an empty target. */
        TargetlessHotkey        = 0x04000000,

        /** Action can be pulled into enclosing menu if it is the only one in
         * its submenu. It may have different text in this case. */
        Pullable                = 0x00400000,

        /** Action is not present in its corresponding menu. */
        HotkeyOnly              = 0x00800000,

        /** Action must have at least one child to appear in menu. */
        RequiresChildren        = 0x01000000,

        /** Action can be executed only in dev-mode. */
        DevMode                 = 0x02000000,

        GlobalHotkey            = NoTarget | ScopelessHotkey | TargetlessHotkey,


        /** Action can appear in main menu. */
        Main                    = MainScope | NoTarget,

        /** Action can appear in scene context menu. */
        Scene                   = SceneScope | WidgetTarget,

        /** Action can appear in scene context menu in videowall review mode (target elements are not resource widgets). */
        VideoWallReviewScene    = SceneScope,

        /** Action can appear in tree context menu. */
        Tree                    = TreeScope,

        /** Action can appear in slider context menu. */
        Slider                  = SliderScope | WidgetTarget,

        /** Action can appear in title bar context menu. */
        TitleBar                = TitleBarScope | LayoutTarget,

        Notifications           = NotificationsScope | WidgetTarget
    };

    Q_DECLARE_FLAGS(ActionFlags, ActionFlag)

    enum ActionVisibility {
        /** Action is not in the menu. */
        InvisibleAction,

        /** Action is in the menu, but is disabled. */
        DisabledAction,

        /** Action is in the menu and can be triggered. */
        EnabledAction
    };

} // namespace Qn

Q_DECLARE_METATYPE(Qn::ActionId);

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ActionScopes);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ActionParameterTypes);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ActionFlags);

#endif // QN_ACTIONS_H
