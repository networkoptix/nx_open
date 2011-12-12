#include "menu_controller.h"
#include <QAction>
#include <cassert>
#include <utils\common\scoped_value_rollback.h>

QnMenuController::QnMenuController(QObject *parent): 
    QObject(parent)
{
    ActionId parentActionId = INVALID_ACTION;
#define DEFINE_ACTION(ACTION, TEXT, SHORTCUT, FLAGS)                            \
    {                                                                           \
        ActionData data;                                                        \
        data.id = ACTION;                                                       \
        data.flags = FLAGS;                                                     \
        data.parentId = parentActionId;                                         \
        data.action = new QAction(TEXT, this);                                  \
        data.action->setShortcut(QKeySequence(SHORTCUT));                       \
        m_infoByAction[ACTION] = data;                                          \
    }
#define DEFINE_SUB_ACTIONS(PARENT)                                              \
    if(QnScopedValueRollback<ActionId> scope = QN_SCOPED_VALUE_ROLLBACK_INITIALIZER(parentActionId, PARENT)) {} else

    DEFINE_ACTION(ITEM_OPEN,                    tr("Open"),                     tr(""),                 TREE_SCOPE);
    DEFINE_ACTION(ITEM_REMOVE,                  tr("Remove"),                   tr(""),                 TREE_SCOPE);
    DEFINE_ACTION(ITEM_BRING_TO_FRONT,          tr("Bring to front"),           tr("Alt+F"),            SCENE_SCOPE);
    DEFINE_ACTION(ITEM_SEND_TO_BACK,            tr("Send to back"),             tr("Alt+B"),            SCENE_SCOPE);
    DEFINE_ACTION(ITEM_PROPERTIES,              tr("Properties"),               tr("Alt+P"),            APP_SCOPE);
    
    DEFINE_ACTION(MEDIA_EDIT_TAGS,              tr("Edit tags"),                tr(""),                 APP_SCOPE | SINGLE_TARGET);
    DEFINE_ACTION(MEDIA_VIEW_IN_FULLSCREEN,     tr("View in full screen"),      tr(""),                 SCENE_SCOPE | SINGLE_TARGET);
    DEFINE_ACTION(MEDIA_TOGGLE_PIN,             tr("Toggle pin"),               tr("Alt+P"),            SCENE_SCOPE);
    DEFINE_ACTION(MEDIA_ROTATE,                 tr("Rotate"),                   tr(""),                 SCENE_SCOPE | IS_MENU);
    DEFINE_SUB_ACTIONS(MEDIA_ROTATE) {
        DEFINE_ACTION(MEDIA_ROTATE_90CW,        tr("Rotate 90 degrees CW"),     tr(""),                 SCENE_SCOPE);
        DEFINE_ACTION(MEDIA_ROTATE_90CCW,       tr("Rotate 90 degrees CCW"),    tr(""),                 SCENE_SCOPE);
        DEFINE_ACTION(MEDIA_ROTATE_180,         tr("Rotate 180 degrees"),       tr(""),                 SCENE_SCOPE);
    }
    
#if 0
    DEFINE_ACTION(LOCAL_OPEN_IN_FOLDER,         APP_SCOPE | SINGLE_TARGET);
    DEFINE_ACTION(LOCAL_DELETE_FROM_DISK,       APP_SCOPE);
    
    DEFINE_ACTION(VIDEO_UPLOAD_TO_YOUTUBE,      APP_SCOPE);

    DEFINE_ACTION(CAMERA_EXPORT_SELECTION,      APP_SCOPE);
    DEFINE_ACTION(CAMERA_START_STOP_RECORDING,  APP_SCOPE);
    DEFINE_ACTION(CAMERA_TAKE_SCREENSHOT,       APP_SCOPE);
    
    DEFINE_ACTION(CAMERA_EXPORT_SELECTION,      APP_SCOPE);
    DEFINE_ACTION(CAMERA_EXPORT_SELECTION,      APP_SCOPE);

    DEFINE_ACTION(LAYOUT_RENAME,                APP_SCOPE);
    DEFINE_ACTION(LAYOUT_ADD,                   APP_SCOPE | IS_MENU);
        DEFINE_ACTION(LAYOUT_ADD_FILES,         APP_SCOPE,                      LAYOUT_ADD);
        DEFINE_ACTION(LAYOUT_ADD_FOLDER,        APP_SCOPE,                      LAYOUT_ADD);
        DEFINE_ACTION(LAYOUT_ADD_CAMERAS,       APP_SCOPE,                      LAYOUT_ADD);

    DEFINE_ACTION(SERVER_NEW_CAMERA,            APP_SCOPE);

    DEFINE_ACTION(SCENE_UNDO,                   SCENE_SCOPE);
    DEFINE_ACTION(SCENE_REDO,                   SCENE_SCOPE);
    DEFINE_ACTION(SCENE_SAVE_LAYOUT,            SCENE_SCOPE);
    DEFINE_ACTION(SCENE_SAVE_LAYOUT_AS,         SCENE_SCOPE);

    DEFINE_ACTION(APP_OPEN_FILE,                APP_SCOPE);
    DEFINE_ACTION(APP_NEW_LAYOUT,               APP_SCOPE);
    DEFINE_ACTION(APP_SYSTEM_SETTINGS,          APP_SCOPE);
    DEFINE_ACTION(APP_FULLSCREEN,               APP_SCOPE);
    DEFINE_ACTION(APP_RECORD,                   APP_SCOPE);
    DEFINE_ACTION(APP_RECORD_START_STOP,        APP_SCOPE);
    DEFINE_ACTION(APP_RECORD_SETTINGS,          APP_SCOPE);
    DEFINE_ACTION(APP_EXIT,                     APP_SCOPE);
#endif
#undef DEFINE_ACTION
#undef DEFINE_SUB_ACTIONS

    assert(m_infoByAction.size() == ACTION_COUNT);

}

QnMenuController::~QnMenuController() {
    return;
}
