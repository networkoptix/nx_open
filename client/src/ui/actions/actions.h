#ifndef QN_ACTIONS_H
#define QN_ACTIONS_H

#include "action_fwd.h"

namespace Qn {

    inline QLatin1String qn_gridPositionParameter()         { return QLatin1String("_qn_gridPosition"); }
    inline QLatin1String qn_userParameter()                 { return QLatin1String("_qn_user"); }
    inline QLatin1String qn_nameParameter()                 { return QLatin1String("_qn_name"); }
    inline QLatin1String qn_serverParameter()               { return QLatin1String("_qn_server"); }
    inline QLatin1String qn_serializedResourcesParameter()  { return QLatin1String("_qn_serializedResourcesParameter"); }
    inline QLatin1String qn_layoutParameter()               { return QLatin1String("_qn_layoutParameter"); }

#define GridPositionParameter qn_gridPositionParameter()
#define UserParameter qn_userParameter()
#define NameParameter qn_nameParameter()
#define ServerParameter qn_serverParameter()
#define LayoutParameter qn_layoutParameter()
#define SerializedResourcesParameter qn_serializedResourcesParameter()

    /**
     * Enum of all menu actions.
     */
    enum ActionId {
        /* Actions that are not assigned to any menu. */

        /**
         * Opens about dialog.
         */
        AboutAction,

        /**
         * Opens connection setting dialog.
         */
        ConnectionSettingsAction,

        /**
         * Reconnects to the application server.
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
         * connection to application server was established.
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
         * Submenu for 'open' commands.
         */
        OpenMenu,

            /**
             * Opens a file dialog and adds selected files to the current layout.
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
         * If not provided, current layout will be used.
         */ 
        OpenInLayoutAction,

        /**
         * Opens selected resources in a new layout.
         */
        OpenInNewLayoutAction,

        /**
         * Opens selected resources in a new window.
         */
        OpenInNewWindowAction,

        /**
         * Opens selected layout.
         */
        OpenSingleLayoutAction,

        /**
         * Opens selected layouts.
         */
        OpenMultipleLayoutsAction,

        /**
         * Opens selected layouts.
         */
        OpenAnyNumberOfLayoutsAction,

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
         * Shows motion search grid on an item.
         */
        ShowMotionAction,

        /**
         * Hides motion search grid on an item.
         */
        HideMotionAction,

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



        /* Layout actions. */

        /**
         * Deletes the file from disk.
         */
        DeleteFromDiskAction,

        /**
         * Removes an item from a layout.
         */
        RemoveLayoutItemAction,

        /**
         * Removes a resource from application server.
         */
        RemoveFromServerAction,

        /**
         * Opens layout name editor.
         */
        RenameLayoutAction,

        /**
         * Opens a user creation dialog.
         */
        NewUserAction,

        /**
         * Opens a layout creation dialog.
         */
        NewUserLayoutAction,



        ActionCount,

        NoAction = -1
    };

    enum ActionScope {
        InvalidScope            = 0x0,
        MainScope               = 0x1,              /**< Application's main menu requested. */
        SceneScope              = 0x2,              /**< Scene context menu requested. */
        TreeScope               = 0x4,              /**< Tree context menu requested. */
        SliderScope             = 0x8,              /**< Slider context menu requested. */
        TabBarScope             = 0x10,             /**< Tab bar context menu requested. */
        ScopeMask               = 0xFF
    };

    enum ActionTarget {
        ResourceTarget          = 0x100,             
        LayoutItemTarget        = 0x200,
        WidgetTarget            = 0x400,
        LayoutTarget            = 0x800,
        TargetMask              = 0xF00
    };

    enum ActionAccessRights {
        EditLayoutRights        = 0x1000,           /** Rights to edit current layout. */
        AdminAccessRights       = 0x2000,           /** Administrative rights. */
        AccessRightsMask        = 0x3000
    };

    enum ActionFlag {
        /** Action can be applied when there are no targets. */
        NoTarget                = 0x10000,           

        /** Action can be applied to a single target. */
        SingleTarget            = 0x20000,           

        /** Action can be applied to multiple targets. */
        MultiTarget             = 0x40000,           


        /** Action accepts resources as target. */
        Resource                = ResourceTarget,   

        /** Action accepts layout items as target. */
        LayoutItem              = LayoutItemTarget, 

        /** Action accepts resource widgets as target. */
        Widget                  = WidgetTarget,     

        /** Action accepts layouts as target. */
        Layout                  = LayoutTarget,     


        /** Action requires administrative access rights. */
        Admin                   = AdminAccessRights,

        /** Action requires the user to be able to edit current layout. */
        EditLayout              = EditLayoutRights,


        /** Action has a hotkey that is intentionally ambiguous. 
         * It is up to the user to ensure that proper action conditions make it 
         * impossible for several actions to be triggered by this hotkey. */
        IntentionallyAmbiguous  = 0x100000,          

        /** When the action is activated via hotkey, its scope should not be compared to the current one. 
         * Action can be executed from any scope, and its target will be taken from its scope. */
        ScopelessHotkey         = 0x200000,       


        /** Action can appear in main menu. */
        Main                    = Qn::MainScope | NoTarget,                     

        /** Action can appear in scene context menu. */
        Scene                   = Qn::SceneScope | Widget,                      

        /** Action can appear in tree context menu. */
        Tree                    = Qn::TreeScope,                                

        /** Action can appear in slider context menu. */
        Slider                  = Qn::SliderScope | SingleTarget | Resource,    

        /** Action can appear in tab bar context menu. */
        TabBar                  = Qn::TabBarScope | SingleTarget | Layout,      
    };

    Q_DECLARE_FLAGS(ActionFlags, ActionFlag);
    
    enum ActionVisibility {
        InvisibleAction,
        DisabledAction,
        EnabledAction
    };

} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ActionFlags);

#endif // QN_ACTIONS_H
