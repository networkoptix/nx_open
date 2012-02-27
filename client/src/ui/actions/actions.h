#ifndef QN_ACTIONS_H
#define QN_ACTIONS_H

namespace Qn {

    /**
     * Enum of all menu actions.
     * 
     * ACHTUNG! Item order is important. 
     * It defines item ordering in the resulting menu.
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
         * Shows / hides FPS display.
         */
        ShowFpsAction,

        /**
         * Opens some existing layout if there exists one, creates a new one otherwise.
         * This action is executed every time the last layout on a workbench is closed.
         */
        OpenSingleLayoutAction,



        /* Main menu actions. */

        /**
         * Opens the main menu.
         */
        MainMenuAction,

        /**
         * Opens a new tab (layout).
         */
        NewLayoutAction,

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

        FileSeparator,

        /**
         * Starts / stops screen recording.
         */
        ScreenRecordingAction,

        /**
         * Toggles client's fullscreen state.
         */
        FullscreenAction,

        /**
         * Opens preferences dialog.
         */
        PreferencesAction,

        ExitSeparator,

        /**
         * Closes the client.
         */
        ExitAction,



        /* Tab bar actions. */

        /**
         * Closes layout.
         */
        CloseLayoutAction,



        /* Resource actions. */

        /**
         * Shows motion search grid on an item.
         */
        ShowMotionAction,

        /**
         * Hides motion search grid on an item.
         */
        HideMotionAction,

        /**
         * Opens camera settings dialog.
         */
        CameraSettingsAction,

        /**
         * Opens multiple cameras settings dialog.
         */
        MultipleCameraSettingsAction,

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

        ItemSeparator,

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
         * Opens a user creation dialog.
         */
        NewUserAction,




#if 0
        /** 
         * Opens the selected layout in the scene. Current layout is closed.
         */
        LAYOUT_OPEN, 

        /**
         * Can be applied only to items inside a layout. 
         * Selected item is removed from layout.
         */
        ITEM_REMOVE_FROM_LAYOUT,

        /**
         * Can be applied only to items on the scene.
         * Brings the selected item(s) to front.
         */
        ITEM_BRING_TO_FRONT,

        /**
         * Can be applied only to items on the scene.
         * Sends selected item(s) to back.
         */
        ITEM_SEND_TO_BACK,

        /**
         * Can be applied to any set of cameras.
         * Opens the properties dialog where properties for the set of selected
         * cameras can be edited.
         */
        CAMERA_PROPERTIES,

        /**
         * Can be applied to any single non-camera item.
         * Opens the item's properties.
         */
        ITEM_PROPERTIES,

        /**
         * Can be applied to any set of items.
         * Opens the tag editing dialog.
         */
        ITEM_EDIT_TAGS,

        /**
         * Can be applied to any single image/video/camera item on the scene.
         * Opens the selected item in full screen.
         */
        MEDIA_VIEW_IN_FULLSCREEN,

        /**
         * Can be applied to an item that is currently viewed in fullscreen mode.
         * Reverts the view back to normal (non-fullscreen) mode.
         */
        MEDIA_VIEW_IN_PREVIEW_MODE,

        /**
         * Can be applied to any set of image/video/camera items on the scene.
         * Toggles the "pinned" state of selected items.
         */
        MEDIA_TOGGLE_PIN,

        /**
         * Can be applied to a set of any set of image/video/camera items on the scene.
         * Rotates selected items as chosen.
         */
        MEDIA_SET_ROTATION,
            MEDIA_SET_ROTATION_0_DEGREES,
            MEDIA_SET_ROTATION_90_DEGREES,
            MEDIA_SET_ROTATION_180_DEGREES,
            MEDIA_SET_ROTATION_270_DEGREES,

        /**
         * Can be applied to any single local item (image, video or folder) 
         * either on the scene or on the tree. Opens the folder that contains
         * selected item in file manager.
         */
        LOCAL_OPEN_IN_FOLDER,

        /**
         * Can be applied to any number of local items (images, videos or folders).
         * Moves selected items to recycle bin.
         */
        LOCAL_DELETE_FROM_DISK,

        /**
         * Can be applied to any number of video items. Opens the YouTube uploading
         * dialog for the selected items.
         */
        VIDEO_UPLOAD_TO_YOUTUBE,

        /**
         * Available when clicking on navigation item with non-empty selection.
         * Opens exporting dialog.
         */
        NAVIGATION_EXPORT_SELECTION,

        /**
         * Can be applied to any number of camera items on the scene. 
         * Opens the export dialog.
         */
        CAMERA_EXPORT,

        /**
         * Can be applied to any number of camera items on the scene or 
         * in the tree menu. Starts camera recording.
         */
        CAMERA_START_RECORDING,

        /**
         * Can be applied to any number of camera items on the scene or 
         * in the tree menu. Stops camera recording.
         */
        CAMERA_STOP_RECORDING,

        /**
         * Can be applied to any number of video and camera items on the scene.
         * Saves screenshot of the item.
         */
        CAMERA_TAKE_SCREENSHOT,

        /**
         * Can be applied to a single server item in the tree menu.
         * Opens manual camera registration dialog.
         */
        SERVER_NEW_CAMERA,

        /**
         * Can be applied to a single layout item in the tree menu and to the
         * current scene layout.
         * Opens camera addition (grid) dialog.
         */
        LAYOUT_ADD_CAMERAS,

        /**
         * Can be applied to a single layout item in the tree menu and to the
         * current scene layout.
         * Opens search dialog for files in root media folder.
         */
        LAYOUT_ADD_FILES, // POSTPONE

        /**
         * Can be applied to a single layout item in the tree menu and to the
         * current scene layout.
         * Opens search dialog for subfolders of the root media folder.
         */
        LAYOUT_ADD_FOLDER, // POSTPONE

        /**
         * Appears only when clicking on the scene.
         * Undoes the last action on the scene.
         */
        SCENE_UNDO,

        /**
         * Appears only when clicking on the scene.
         * Redoes the last undone action on the scene.
         */
        SCENE_REDO,

        /**
         * Appears only when clicking on the scene. Enabled if current layout
         * has unsaved changes. Saves the current layout.
         */
        SCENE_SAVE_LAYOUT,

        /**
         * Appears only when clicking on the scene.
         * Opens the "Save as..." dialog.
         */
        SCENE_SAVE_LAYOUT_AS,

        /**
         * Appears when clicking on the scene.
         * Opens OS file selection dialog. Selected files are added to the scene.
         */
        APP_OPEN_FILES,

        /**
         * Appears when clicking on the tree menu. 
         * Creates new empty layout that can be renamed and opened.
         */
        APP_NEW_LAYOUT,

        /**
         * Opens system settings dialog. Available everywhere.
         */ 
        APP_SYSTEM_SETTINGS,

        /**
         * Toggles application fullscreen mode. Available everywhere.
         */
        APP_FULLSCREEN,

        APP_RECORD,
            /**
             * Toggles screen recording. Available everywhere.
             */
            APP_RECORD_START_STOP,

            /**
             * Opens screen recording dialog. Available everywhere.
             */
            APP_RECORD_SETTINGS,

        /**
         * Closes client. Available everywhere.
         */
        APP_EXIT,

#endif

        ActionCount,

        NoAction = -1
    };

    enum ActionScope {
        InvalidScope        = 0x0,
        MainScope           = 0x1,              /**< Application's main menu requested. */
        SceneScope          = 0x2,              /**< Scene context menu requested. */
        TreeScope           = 0x4,              /**< Tree context menu requested. */
        SliderScope         = 0x8,              /**< Slider context menu requested. */
        TabBarScope         = 0x10,             /**< Tab bar context menu requested. */
        ScopeMask           = 0xFF
    };

    enum ActionTarget {
        ResourceTarget      = 0x100,             
        LayoutItemTarget    = 0x200,
        WidgetTarget        = 0x400,
        LayoutTarget        = 0x800,
        TargetMask          = 0xF00
    };

    enum ActionFlag {
        /** Action can be applied when there are no targets. */
        NoTarget                = 0x1000,           

        /** Action can be applied to a single target. */
        SingleTarget            = 0x2000,           

        /** Action can be applied to multiple targets. */
        MultiTarget             = 0x4000,           


        /** Action accepts resources as target. */
        Resource                = ResourceTarget,   

        /** Action accepts layout items as target. */
        LayoutItem              = LayoutItemTarget, 

        /** Action accepts resource widgets as target. */
        Widget                  = WidgetTarget,     

        /** Action accepts layouts as target. */
        Layout                  = LayoutTarget,     


        /** Action has a hotkey that is intentionally ambiguous. 
         * It is up to the user to ensure that proper action conditions make it 
         * impossible for several actions to be triggered by this hotkey. */
        IntentionallyAmbiguous  = 0x10000,          

        /** When the action is activated via hotkey, its scope should not be compared to the current one. 
         * Action can be executed from any scope, and its target will be taken from its scope. */
        ScopelessHotkey         = 0x20000,       


        /** Action cannot appear in any menu. */
        Invisible               = 0,                                            
        
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

} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ActionFlags);

#endif // QN_ACTIONS_H
