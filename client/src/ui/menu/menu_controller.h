#ifndef QN_MENU_CONTROLLER_H
#define QN_MENU_CONTROLLER_H

#include <QObject>
#include <QHash>

class QAction;

namespace detail {
    class ActionFactory;
}

class QnMenuController: public QObject {
    Q_OBJECT;
public:
    QnMenuController(QObject *parent = NULL);

    virtual ~QnMenuController();

    /**
     * Enum of all menu actions.
     * 
     * Note that item order is important. It defines item ordering in the
     * resulting menu.
     */
    enum ActionId {
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

        ACTION_COUNT,

        INVALID_ACTION = -1
    };

    enum ActionFlag {
        SCENE_SCOPE         = 0x00000001,   /**< Action appears in scene menu. */
        TREE_SCOPE          = 0x00000002,   /**< Action appears in tree menu. */
        NAVIGATION_SCOPE    = 0x00000004,   /**< Action appears in navigation item's menu. */
        GLOBAL_SCOPE        = NAVIGATION_SCOPE | SCENE_SCOPE | TREE_SCOPE,
        
        IMAGE_TARGET        = 0x00000100,   /**< Action is applicable to still images. */
        VIDEO_TARGET        = 0x00000200,   /**< Action is applicable to videos. */
        CAMERA_TARGET       = 0x00000400,   /**< Action is applicable to cameras. */
        SERVER_TARGET       = 0x00000800,   /**< Action is applicable to servers. */
        LAYOUT_TARGET       = 0x00001000,   /**< Action is applicable to layouts. */
        FOLDER_TARGET       = 0x00002000,   /**< Action is applicable to folders. */
        EMPTY_TARGET        = 0x00004000,   /**< Action is applicable even when there are no targets. */
        MEDIA_TARGET        = VIDEO_TARGET | IMAGE_TARGET,
        
        NO_MULTI_TARGET     = 0x00010000,   /**< Action cannot be applied to multiple targets. */
        //NO_REMOTE_TARGET    = 0x00020000,   /**< Action cannot be applied to non-local targets. */

        IS_MENU             = 0x01000000,   /**< Action is actually a menu with sub-actions. */
        IS_SEPARATOR        = 0x02000000,   /**< Action is actually a menu separator. */
    };

    Q_DECLARE_FLAGS(ActionFlags, ActionFlag);

protected:
    friend class detail::ActionFactory;

    struct ActionData {
        ActionId id;
        ActionFlags flags;
        ActionId parentId;
        QAction *action;
    };

private:
    QHash<ActionId, ActionData> m_infoByAction;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnMenuController::ActionFlags);

#endif // QN_MENU_CONTROLLER_H
