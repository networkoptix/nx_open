#ifndef QN_ACTIONS_H
#define QN_ACTIONS_H

#include "action_fwd.h"

namespace Qn {

    inline QLatin1String qn_gridPositionParameter()         { return QLatin1String("_qn_gridPosition"); }
    inline QLatin1String qn_userParameter()                 { return QLatin1String("_qn_user"); }
    inline QLatin1String qn_nameParameter()                 { return QLatin1String("_qn_name"); }
    inline QLatin1String qn_serverParameter()               { return QLatin1String("_qn_server"); }
    inline QLatin1String qn_layoutParameter()               { return QLatin1String("_qn_layoutParameter"); }
    inline QLatin1String qn_currentLayoutParameter()        { return QLatin1String("_qn_currentLayoutParameter"); }
    inline QLatin1String qn_currentUserParameter()          { return QLatin1String("_qn_currentUserParameter"); }
    inline QLatin1String qn_serializedResourcesParameter()  { return QLatin1String("_qn_serializedResourcesParameter"); }
    inline QLatin1String qn_timePeriodParameter()           { return QLatin1String("_qn_timePeriodParameter"); }
    inline QLatin1String qn_timePeriodsParameter()          { return QLatin1String("_qn_timePeriodsParameter"); }
    inline QLatin1String qn_connectInfoParameter()          { return QLatin1String("_qn_connectInfoParameter"); }

#define GridPositionParameter qn_gridPositionParameter()
#define UserParameter qn_userParameter()
#define NameParameter qn_nameParameter()
#define ServerParameter qn_serverParameter()
#define LayoutParameter qn_layoutParameter()
#define CurrentLayoutParameter qn_currentLayoutParameter()
#define CurrentUserParameter qn_currentUserParameter()
#define SerializedResourcesParameter qn_serializedResourcesParameter()
#define TimePeriodParameter qn_timePeriodParameter()
#define TimePeriodsParameter qn_timePeriodsParameter()
#define ConnectInfoParameter qn_connectInfoParameter()

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
         * <tt>QnVideoServerResourcePtr ServerParameter</tt> --- video server to
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
        LightMainMenuAction,
        DarkMainMenuAction,

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
        ScreenRecordingAction,

        /**
         * Toggles client's fullscreen state.
         */
        FullscreenAction,

        /**
         * Goes to fullscreen and slides out all panels.
         */
        FreespaceAction,

        /**
         * Minimizes client's main window.
         */ 
        MinimizeAction,

        /**
         * Maximizes client's main window.
         */ 
        MaximizeAction,

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
         * Closes all layouts but selected.
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
         * Opens server settings dialog.
         */
        ServerSettingsAction,

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
         * Shows/hides thumbnails.
         */
        ToggleThumbnailsAction,

        /**
         * Opens new layout for Quick Search.
         * 
         * Parameters:
         * 
         * <tt>QnTimePeriod TimePeriodParameter</tt> --- time period for quick search.
         */
        QuickSearchAction,

        /**
         * Shows/hides calendar.
         */
        ToggleCalendarAction,


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
        InvalidScope            = 0x0,
        MainScope               = 0x1,              /**< Action appears in main menu. */
        SceneScope              = 0x2,              /**< Action appears in scene context menu and its parameters are taken from the scene. */
        TreeScope               = 0x4,              /**< Action appears in tree context menu. */
        SliderScope             = 0x8,              /**< Action appears in slider context menu. */
        TabBarScope             = 0x10,             /**< Action appears tab bar context menu. */
        ScopeMask               = 0xFF
    };
    Q_DECLARE_FLAGS(ActionScopes, ActionScope);

    /**
     * Type of an action parameter.
     * 
     * Note that some of these types are convertible to other types. 
     */
    enum ActionParameterType {
        ResourceType            = 0x0100,           /**< Resource, <tt>QnResourcePtr</tt>. */
        LayoutItemType          = 0x0200,           /**< Layout item, <tt>QnLayoutItemIndex</tt>. Convertible to resource. */    
        WidgetType              = 0x0400,           /**< Resource widget, <tt>QnResourceWidget *</tt>. Convertible to layout item and resource. */
        LayoutType              = 0x0800,           /**< Workbench layout, <tt>QnWorkbenchLayout *</tt>. Convertible to resource. */
        OtherType               = 0x1000,           /**< Some other type. */
        TargetTypeMask          = 0xFF00
    };
    Q_DECLARE_FLAGS(ActionParameterTypes, ActionParameterType)

    enum ActionFlag {
        /** Action can be applied when there are no targets. */
        NoTarget                = 0x10000,           

        /** Action can be applied to a single target. */
        SingleTarget            = 0x20000,           

        /** Action can be applied to multiple targets. */
        MultiTarget             = 0x40000,           

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
        IntentionallyAmbiguous  = 0x100000,          

        /** When the action is activated via hotkey, its scope should not be compared to the current one. 
         * Action can be executed from any scope, and its target will be taken from its scope. */
        ScopelessHotkey         = 0x200000,       

        /** Action can be pulled into enclosing menu if it is the only one in
         * its submenu. It may have different text in this case. */
        Pullable                = 0x400000,

        /** Action is not present in its corresponding menu. */
        HotkeyOnly              = 0x800000,


        /** Action can appear in main menu. */
        Main                    = Qn::MainScope | NoTarget,                     

        /** Action can appear in scene context menu. */
        Scene                   = Qn::SceneScope | WidgetTarget,                      

        /** Action can appear in tree context menu. */
        Tree                    = Qn::TreeScope,                                

        /** Action can appear in slider context menu. */
        Slider                  = Qn::SliderScope | WidgetTarget,    

        /** Action can appear in tab bar context menu. */
        TabBar                  = Qn::TabBarScope | LayoutTarget,      
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
