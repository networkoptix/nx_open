#include "menu_controller.h"
#include <QAction>
#include <cassert>

namespace detail {
    class ActionFactory {
    public:
        ActionFactory(QnMenuController *controller): m_controller(controller) {
            m_parentStack.push_back(QnMenuController::INVALID_ACTION);
        }

        void pushParent(QnMenuController::ActionId parentId) {
            m_parentStack.push_back(parentId);
        }

        void popParent() {
            m_parentStack.pop_back();
        }

        void operator()(QnMenuController::ActionId id, const QString &text, const QString &shortcut, QnMenuController::ActionFlags flags) {
            QnMenuController::ActionData data;
            data.id = id;
            data.flags = flags;
            data.parentId = m_parentStack.back();
            data.action = new QAction(text, m_controller);
            data.action->setShortcut(QKeySequence(shortcut));
            m_controller->m_infoByAction[id] = data;
        }

    private:
        QnMenuController *m_controller;
        QVector<QnMenuController::ActionId> m_parentStack;
    };

} // namespace detail


QnMenuController::QnMenuController(QObject *parent): 
    QObject(parent)
{
    detail::ActionFactory factory(this);

    factory(ITEM_OPEN,                      tr("Open"),                         tr(""),                 TREE_SCOPE);
    factory(ITEM_REMOVE,                    tr("Remove"),                       tr(""),                 TREE_SCOPE);
    factory(ITEM_BRING_TO_FRONT,            tr("Bring to front"),               tr("Alt+F"),            SCENE_SCOPE);
    factory(ITEM_SEND_TO_BACK,              tr("Send to back"),                 tr("Alt+B"),            SCENE_SCOPE);
    factory(ITEM_PROPERTIES,                tr("Properties"),                   tr("Alt+P"),            APP_SCOPE);
    
    factory(MEDIA_EDIT_TAGS,                tr("Edit tags"),                    tr(""),                 APP_SCOPE | SINGLE_TARGET);
    factory(MEDIA_VIEW_IN_FULLSCREEN,       tr("View in full screen"),          tr(""),                 SCENE_SCOPE | SINGLE_TARGET);
    factory(MEDIA_TOGGLE_PIN,               tr("Toggle pin"),                   tr("Alt+P"),            SCENE_SCOPE);
    factory(MEDIA_ROTATE,                   tr("Rotate..."),                    tr(""),                 SCENE_SCOPE | IS_MENU);
    factory.pushParent(MEDIA_ROTATE);
        factory(MEDIA_ROTATE_90CW,          tr("Rotate 90 degrees CW"),         tr(""),                 SCENE_SCOPE);
        factory(MEDIA_ROTATE_90CCW,         tr("Rotate 90 degrees CCW"),        tr(""),                 SCENE_SCOPE);
        factory(MEDIA_ROTATE_180,           tr("Rotate 180 degrees"),           tr(""),                 SCENE_SCOPE);
    factory.popParent();
    
    factory(LOCAL_OPEN_IN_FOLDER,           tr("Open in folder"),               tr(""),                 APP_SCOPE | SINGLE_TARGET);
    factory(LOCAL_DELETE_FROM_DISK,         tr("Delete from disk"),             tr(""),                 APP_SCOPE);
    
    factory(VIDEO_UPLOAD_TO_YOUTUBE,        tr("Upload to YouTube"),            tr(""),                 APP_SCOPE);

    factory(CAMERA_EXPORT_SELECTION,        tr("Export selection"),             tr(""),                 APP_SCOPE);
    factory(CAMERA_START_STOP_RECORDING,    tr("Start/stop recording"),         tr(""),                 APP_SCOPE);
    factory(CAMERA_TAKE_SCREENSHOT,         tr("Take screenshot"),              tr(""),                 APP_SCOPE);
    
    factory(LAYOUT_RENAME,                  tr("Rename"),                       tr(""),                 APP_SCOPE);
    factory(LAYOUT_ADD,                     tr("Add..."),                       tr(""),                 APP_SCOPE | IS_MENU);
    factory.pushParent(LAYOUT_ADD);
        factory(LAYOUT_ADD_FILES,           tr("Add files"),                    tr(""),                 APP_SCOPE);
        factory(LAYOUT_ADD_FOLDER,          tr("Add folder"),                   tr(""),                 APP_SCOPE);
        factory(LAYOUT_ADD_CAMERAS,         tr("Add cameras"),                  tr(""),                 APP_SCOPE);
    factory.popParent();

    factory(SERVER_NEW_CAMERA,              tr("New camera"),                   tr(""),                 APP_SCOPE);

    factory(SCENE_UNDO,                     tr("Undo"),                         tr(""),                 SCENE_SCOPE);
    factory(SCENE_REDO,                     tr("Redo"),                         tr(""),                 SCENE_SCOPE);
    factory(SCENE_SAVE_LAYOUT,              tr("Save layout"),                  tr(""),                 SCENE_SCOPE);
    factory(SCENE_SAVE_LAYOUT_AS,           tr("Save layout as..."),            tr(""),                 SCENE_SCOPE);

    factory(APP_OPEN_FILE,                  tr("Open file"),                    tr(""),                 APP_SCOPE);
    factory(APP_NEW_LAYOUT,                 tr("New layout"),                   tr(""),                 APP_SCOPE);
    factory(APP_SYSTEM_SETTINGS,            tr("System settings"),              tr(""),                 APP_SCOPE);
    factory(APP_FULLSCREEN,                 tr("Fullscreen"),                   tr(""),                 APP_SCOPE);
    factory(APP_RECORD,                     tr("Screen recording..."),          tr(""),                 APP_SCOPE);
    factory(APP_RECORD_START_STOP,          tr("Start/stop screen recording"),  tr(""),                 APP_SCOPE);
    factory(APP_RECORD_SETTINGS,            tr("Screen recording settings"),    tr(""),                 APP_SCOPE);
    factory(APP_EXIT,                       tr("Exit"),                         tr(""),                 APP_SCOPE);

    assert(m_infoByAction.size() == ACTION_COUNT);
}

QnMenuController::~QnMenuController() {
    return;
}
