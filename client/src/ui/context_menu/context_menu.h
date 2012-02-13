#ifndef QN_CONTEXT_MENU_H
#define QN_CONTEXT_MENU_H

#include <QObject>
#include <QHash>
#include <QVariant>
#include <core/resource/resource_fwd.h>

class QAction;
class QMenu;
class QGraphicsItem;

class QnActionFactory;
class QnActionBuilder;
class QnActionCondition;

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
         * Opens a new tab (layout).
         */
        NewTabAction,

        /**
         * Closes current tab (layout).
         */
        CloseTabAction,



        /* Main menu actions. */

        /**
         * Opens the main menu.
         */
        MainMenuAction,

        /**
         * Opens a file dialog and adds selected files to the current layout.
         */
        OpenFileAction,

        /**
         * Submenu for screen recording.
         */
        ScreenRecordingMenu,

            /**
             * Starts / stops screen recording.
             */
            ScreenRecordingAction,

            /**
             * Opens screen recording dialog.
             */
            ScreenRecordingSettingsAction,

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
        MainScope           = 0x1,              /**< Application's main menu requested. */
        SceneScope          = 0x2,              /**< Scene context menu requested. */
        TreeScope           = 0x4,              /**< Tree context menu requested. */
        SliderScope         = 0x8,              /**< Slider context menu requested. */
    };

} // namespace Qn


class QnContextMenu: public QObject {
    Q_OBJECT;
public:
    enum ActionFlag {
        NoTarget            = 0x00000010,       /**< Action can be applied when there are no targets. */
        SingleTarget        = 0x00000020,       /**< Action can be applied to a single target. */
        MultiTarget         = 0x00000040,       /**< Action can be applied to multiple targets. */
        TargetMask          = 0x00000070,

        Invisible           = 0,                                /**< Action cannot appear in any menu. */
        Main                = Qn::MainScope | NoTarget,         /**< Action can appear in main menu. */
        Scene               = Qn::SceneScope,                   /**< Action can appear in scene context menu. */
        Tree                = Qn::TreeScope,                    /**< Action can appear in tree context menu. */
        Slider              = Qn::SliderScope | SingleTarget,   /**< Action can appears in slider context menu. */
        Global              = Scene | Tree | Slider, 

        ToggledText         = 0x01000000,       /**< Action has different text for toggled state. */
        Separator           = 0x10000000,       /**< Action is actually a menu separator. */
    };

    Q_DECLARE_FLAGS(ActionFlags, ActionFlag);

    QnContextMenu(QObject *parent = NULL);

    virtual ~QnContextMenu();

    static QnContextMenu *instance();

    QAction *action(Qn::ActionId id) const;

    QMenu *newMenu(Qn::ActionScope scope) const;

    QMenu *newMenu(Qn::ActionScope scope, const QnResourceList &resources) const;

    QMenu *newMenu(Qn::ActionScope scope, const QList<QGraphicsItem *> &items) const;

protected:
    friend class QnActionFactory;
    friend class QnActionBuilder;

    struct ActionData {
        ActionData(): id(Qn::NoAction), flags(0), action(NULL), condition(NULL) {}

        Qn::ActionId id;
        ActionFlags flags;
        QAction *action;
        QString normalText, toggledText;
        QnActionCondition *condition;

        QList<ActionData *> children;
    };

    template<class ItemSequence>
    QMenu *newMenuInternal(const ActionData *parent, Qn::ActionScope scope, const ItemSequence &items) const;

private slots:
    void at_action_toggled();

private:
    QHash<Qn::ActionId, ActionData *> m_dataById;
    ActionData *m_root;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnContextMenu::ActionFlags);


#define qnMenu          (QnContextMenu::instance())
#define qnAction(id)    (QnContextMenu::instance()->action(id))

#endif // QN_CONTEXT_MENU_H
