#ifndef QN_ACTIONS_H
#define QN_ACTIONS_H

#include "action_fwd.h"

namespace Qn {

    inline QLatin1String fromLatin1(const char *s) {
        return QLatin1String(s);
    }

#define GridPositionParameter               fromLatin1("_qn_gridPosition")
#define UserParameter                       fromLatin1("_qn_user")
#define NameParameter                       fromLatin1("_qn_name")
#define ServerParameter                     fromLatin1("_qn_server")
#define LayoutParameter                     fromLatin1("_qn_layoutParameter")
#define CurrentLayoutParameter              fromLatin1("_qn_currentLayoutParameter")
#define CurrentLayoutMediaItemsParameter    fromLatin1("_qn_currentLayoutMediaItemsParameter")
#define CurrentUserParameter                fromLatin1("_qn_currentUserParameter")
#define AllMediaServersParameter            fromLatin1("_qn_allMediaServers")
#define SerializedResourcesParameter        fromLatin1("_qn_serializedResourcesParameter")
#define TimePeriodParameter                 fromLatin1("_qn_timePeriodParameter")
#define TimePeriodsParameter                fromLatin1("_qn_timePeriodsParameter")
#define ConnectInfoParameter                fromLatin1("_qn_connectInfoParameter")

    /**
     * Enum of all menu actions.
     */
    enum ActionId {
        /* Actions that are not assigned to any menu. */

        /**
         * Opens connection setting dialog.
         */
        ConnectToServerAction,

        /**
         * Opens licenses preferences tab.
         */
        GetMoreLicensesAction,

        /**
         * Reconnects to the Enterprise Controller using the last used URL
         * set in <tt>QnSettings</tt>.
         * 
         * Parameters.
         * <tt>QnConnectInfoPtr ConnectInfoParameter</tt> --- a connection info
         * to use. If not provided, action handler will try to send a connect
         * request first.
         */
        ReconnectAction,

        /**
         * Shows / hides FPS display.
         */
        ShowFpsAction,

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
         * connection to Enterprise Controller was established.
         * 
         * Parameters:
         * 
         * <tt>QByteArray SerializedResourcesParameter</tt> --- a serialized
         * QnMimeData representation of a set of resources.
         */ 
        DelayedDropResourcesAction,

        /**
         * Moves cameras from one server to another.
         * 
         * Parameters.
         * 
         * <tt>QnMediaServerResourcePtr ServerParameter</tt> --- video server to
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
        SystemSettingsAction,

        /**
         * Opens about dialog.
         */
        AboutAction,

        /**
         * Closes the client.
         */
        ExitAction,



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
         * <tt>QPointF GridPositionParameter</tt> --- drop position, in grid coordinates. 
         * If not provided, Items will be dropped at the center of the layout.
         * <tt>QnLayoutResourcePtr LayoutParameter</tt> --- layout to drop at.
         */ 
        OpenInLayoutAction,

        /**
         * Opens selected resources in current layout.
         * 
         * Parameters:
         * 
         * <tt>QPointF GridPositionParameter</tt> --- drop position, in grid coordinates. 
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
         * Menu containing all layouts belonging to the current user.
         */
        OpenCurrentUserLayoutMenu,

        /**
         * Opens selected layouts in a new window.
         */
        OpenNewWindowLayoutsAction,

        /**
         * Saves selected layout.
         */
        SaveLayoutAction,

        /**
         * Saves selected layout under another name.
         * 
         * Parameters:
         * 
         * <tt>QnUserResourcePtr UserParameter</tt> --- user to assign layout to.
         * <tt>QString NameParameter</tt> --- name for the new layout.
         */
        SaveLayoutAsAction,

        /**
         * Saves selected layout under another name in current user's layouts list.
         * 
         * Parameters:
         * <tt>QString NameParameter</tt> --- name for the new layout.
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
         */
        TakeScreenshotAction,

        /**
         * Opens user settings dialog.
         */
        UserSettingsAction,

        /**
         * Opens camera settings dialog.
         */
        CameraSettingsAction,

        /**
         * Opens provided resources in an existing camera settings dialog.
         */
        OpenInCameraSettingsDialogAction,

        /**
         * Clears the resource that is currently open in camera settings dialog.
         */
        ClearCameraSettingsAction,

        /**
         * Opens server settings dialog.
         */
        ServerSettingsAction,

        /**
         * Opens manual camera addition dialog.
         */
        ServerAddCameraManuallyAction,

        /**
         * Opens a YouTube upload dialog.
         */
        YouTubeUploadAction,

        /**
         * Opens tags editing dialog.
         */
        EditTagsAction,

        /**
         * Opens a folder that contains the file resource.
         */
        OpenInFolderAction,

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

        /* Layout actions. */

        /**
         * Deletes the file from disk.
         */
        DeleteFromDiskAction,

        /**
         * Removes item(s) from layout(s).
         */
        RemoveLayoutItemAction,

        /**
         * Removes a resource from Enterprise Controller.
         */
        RemoveFromServerAction,

        /**
         * Changes resource name.
         * 
         * Parameters:
         * 
         * <tt>QString NameParameter</tt> --- new name for the resource. If not
         * supplied, name dialog will pop up.
         */
        RenameAction,

        /**
         * Opens a user creation dialog.
         */
        NewUserAction,

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
         * Exports selected range.
         */
        ExportTimeSelectionAction,

        /**
         * Exports whole layout.
         */
        ExportLayoutAction,

        /**
         * Opens new layout for Quick Search.
         * 
         * Parameters:
         * 
         * <tt>QnTimePeriod TimePeriodParameter</tt> --- time period for quick search.
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
         * Shows/hides slider.
         */
        ToggleSliderAction,


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


        /* Debug actions. */

        /**
         * Increments debug counter.
         */
        IncrementDebugCounterAction,

        /**
         * Decrements debug counter.
         */
        DecrementDebugCounterAction,


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
        OtherType               = 0x00001000,           /**< Some other type. */
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


        /** Action has a hotkey that is intentionally ambiguous. 
         * It is up to the user to ensure that proper action conditions make it 
         * impossible for several actions to be triggered by this hotkey. */
        IntentionallyAmbiguous  = 0x00100000,          

        /** When the action is activated via hotkey, its scope should not be compared to the current one. 
         * Action can be executed from any scope, and its target will be taken from its scope. */
        ScopelessHotkey         = 0x00200000,       

        /** Action can be pulled into enclosing menu if it is the only one in
         * its submenu. It may have different text in this case. */
        Pullable                = 0x00400000,

        /** Action is not present in its corresponding menu. */
        HotkeyOnly              = 0x00800000,

        /** Action must have at least one child to appear in menu. */
        RequiresChildren        = 0x01000000,


        /** Action can appear in main menu. */
        Main                    = Qn::MainScope | NoTarget,                     

        /** Action can appear in scene context menu. */
        Scene                   = Qn::SceneScope | WidgetTarget,                      

        /** Action can appear in tree context menu. */
        Tree                    = Qn::TreeScope,                                

        /** Action can appear in slider context menu. */
        Slider                  = Qn::SliderScope | WidgetTarget,    

        /** Action can appear in title bar context menu. */
        TitleBar                = Qn::TitleBarScope | LayoutTarget,      
    };

    Q_DECLARE_FLAGS(ActionFlags, ActionFlag);
    
    enum ActionVisibility {
        /** Action is not in the menu. */
        InvisibleAction,

        /** Action is in the menu, but is disabled. */
        DisabledAction,

        /** Action is in the menu and can be triggered. */
        EnabledAction
    };

} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ActionScopes);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ActionParameterTypes);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ActionFlags);

#endif // QN_ACTIONS_H
