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
        SCENE_SCOPE     = 0x0001,   /**< ActionId appears in scene menu. */
        TREE_SCOPE      = 0x0002,   /**< ActionId appears in tree menu. */
        SINGLE_TARGET   = 0x0010,   /**< ActionId appears only when menu is requested for a single target. */
        IS_MENU         = 0x0020,   /**< ActionId is actually a menu with sub-actions. */
        IS_SEPARATOR    = 0x0040,   /**< ActionId is actually a menu separator. */

        APP_SCOPE       = SCENE_SCOPE | TREE_SCOPE,
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
