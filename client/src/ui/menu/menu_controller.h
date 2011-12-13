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
        ITEM_OPEN,                      /**< Opens item in scene. */
        ITEM_REMOVE,                    
        ITEM_BRING_TO_FRONT,
        ITEM_SEND_TO_BACK,
        ITEM_PROPERTIES,

        MEDIA_EDIT_TAGS,
        MEDIA_VIEW_IN_FULLSCREEN,
        MEDIA_TOGGLE_PIN,
        MEDIA_ROTATE,
            MEDIA_ROTATE_90CW,
            MEDIA_ROTATE_90CCW,
            MEDIA_ROTATE_180,

        LOCAL_OPEN_IN_FOLDER,
        LOCAL_DELETE_FROM_DISK,

        VIDEO_UPLOAD_TO_YOUTUBE,
        
        CAMERA_EXPORT_SELECTION,
        CAMERA_START_STOP_RECORDING,
        CAMERA_TAKE_SCREENSHOT,

        LAYOUT_RENAME,
        LAYOUT_ADD,
            LAYOUT_ADD_FILES,
            LAYOUT_ADD_FOLDER,
            LAYOUT_ADD_CAMERAS,

        SERVER_NEW_CAMERA,

        SCENE_UNDO,
        SCENE_REDO,
        SCENE_SAVE_LAYOUT,
        SCENE_SAVE_LAYOUT_AS,

        APP_OPEN_FILE,
        APP_NEW_LAYOUT,
        APP_SYSTEM_SETTINGS,
        APP_FULLSCREEN,
        APP_RECORD,
            APP_RECORD_START_STOP,
            APP_RECORD_SETTINGS,
        APP_EXIT,

        ACTION_COUNT,

        INVALID_ACTION = -1
    };

    enum ActionFlag {
        SCENE_SCOPE         = 0x00000001,   /**< Action appears in scene menu. */
        TREE_SCOPE          = 0x00000002,   /**< Action appears in tree menu. */
        GLOBAL_SCOPE        = SCENE_SCOPE | TREE_SCOPE,
        
        IMAGE_TARGET        = 0x00000100,   /**< Action is applicable to still images. */
        VIDEO_TARGET        = 0x00000200,   /**< Action is applicable to videos. */
        CAMERA_TARGET       = 0x00000400,   /**< Action is applicable to cameras. */
        SERVER_TARGET       = 0x00000800,   /**< Action is applicable to servers. */
        LAYOUT_TARGET       = 0x00001000,   /**< Action is applicable to layouts. */
        EMPTY_TARGET        = 0x00002000,   /**< Action is applicable even when there are no targets. */
        MEDIA_TARGET        = VIDEO_TARGET | IMAGE_TARGET,
        
        NO_MULTI_TARGET     = 0x00010000,   /**< Action cannot be applied to multiple targets. */
        NO_REMOTE_TARGET    = 0x00020000,   /**< Action cannot be applied to non-local targets. */

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
