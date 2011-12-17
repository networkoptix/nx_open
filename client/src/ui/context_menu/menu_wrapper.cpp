#include "menu_wrapper.h"
#include <QAction>
#include <cassert>

namespace detail {
    class ActionFactory {
    public:
        ActionFactory(QnMenuWrapper *controller): m_controller(controller) {
            m_parentStack.push_back(QnMenuWrapper::INVALID_ACTION);
        }

        void pushParent(QnMenuWrapper::ActionId parentId) {
            m_parentStack.push_back(parentId);
        }

        void popParent() {
            m_parentStack.pop_back();
        }

        void operator()(QnMenuWrapper::ActionId id, const QString &text, const QString &shortcut, QnMenuWrapper::ActionFlags flags) {
            QnMenuWrapper::ActionData data;
            data.id = id;
            data.flags = flags;
            data.parentId = m_parentStack.back();
            data.action = new QAction(text, m_controller);
            data.action->setShortcut(QKeySequence(shortcut));
            m_controller->m_infoByAction[id] = data;
        }

    private:
        QnMenuWrapper *m_controller;
        QVector<QnMenuWrapper::ActionId> m_parentStack;
    };

} // namespace detail


QnMenuWrapper::QnMenuWrapper(QObject *parent): 
    QObject(parent)
{
    detail::ActionFactory factory(this);

    //factory(ITEM_OPEN,                      tr("Open"),                         tr(""),                 TREE_SCOPE);
    //factory(ITEM_REMOVE,                    tr("Remove"),                       tr(""),                 TREE_SCOPE);
    factory(ITEM_BRING_TO_FRONT,            tr("Bring to front"),               tr("Alt+F"),            SCENE_SCOPE);
    factory(ITEM_SEND_TO_BACK,              tr("Send to back"),                 tr("Alt+B"),            SCENE_SCOPE);
    factory(ITEM_PROPERTIES,                tr("Properties"),                   tr("Alt+P"),            GLOBAL_SCOPE);
    
    //factory(MEDIA_EDIT_TAGS,                tr("Edit tags"),                    tr(""),                 GLOBAL_SCOPE | NO_MULTI_TARGET);
    factory(MEDIA_VIEW_IN_FULLSCREEN,       tr("View in full screen"),          tr(""),                 SCENE_SCOPE | NO_MULTI_TARGET);
    factory(MEDIA_TOGGLE_PIN,               tr("Toggle pin"),                   tr("Alt+P"),            SCENE_SCOPE); // TODO: duplicate hotkey
    //factory(MEDIA_ROTATE,                   tr("Rotate..."),                    tr(""),                 SCENE_SCOPE | IS_MENU);
    //factory.pushParent(MEDIA_ROTATE);
      //  factory(MEDIA_ROTATE_90CW,          tr("Rotate 90 degrees CW"),         tr(""),                 SCENE_SCOPE);
        //factory(MEDIA_ROTATE_90CCW,         tr("Rotate 90 degrees CCW"),        tr(""),                 SCENE_SCOPE);
        //factory(MEDIA_ROTATE_180,           tr("Rotate 180 degrees"),           tr(""),                 SCENE_SCOPE);
    factory.popParent();
    
    factory(LOCAL_OPEN_IN_FOLDER,           tr("Open in folder"),               tr(""),                 GLOBAL_SCOPE | NO_MULTI_TARGET);
    factory(LOCAL_DELETE_FROM_DISK,         tr("Delete from disk"),             tr(""),                 GLOBAL_SCOPE);
    
    factory(VIDEO_UPLOAD_TO_YOUTUBE,        tr("Upload to YouTube"),            tr(""),                 GLOBAL_SCOPE);

    //factory(CAMERA_EXPORT_SELECTION,        tr("Export selection"),             tr(""),                 GLOBAL_SCOPE);
    //factory(CAMERA_START_STOP_RECORDING,    tr("Start/stop recording"),         tr(""),                 GLOBAL_SCOPE);
    factory(CAMERA_TAKE_SCREENSHOT,         tr("Take screenshot"),              tr(""),                 GLOBAL_SCOPE);
    
    //factory(LAYOUT_RENAME,                  tr("Rename"),                       tr(""),                 TREE_SCOPE);
    //factory(LAYOUT_ADD,                     tr("Add..."),                       tr(""),                 GLOBAL_SCOPE | IS_MENU);
    //factory.pushParent(LAYOUT_ADD);
        factory(LAYOUT_ADD_FILES,           tr("Add files"),                    tr(""),                 GLOBAL_SCOPE);
        factory(LAYOUT_ADD_FOLDER,          tr("Add folder"),                   tr(""),                 GLOBAL_SCOPE);
        factory(LAYOUT_ADD_CAMERAS,         tr("Add cameras"),                  tr(""),                 GLOBAL_SCOPE);
    //factory.popParent();

    factory(SERVER_NEW_CAMERA,              tr("New camera"),                   tr(""),                 GLOBAL_SCOPE);

    factory(SCENE_UNDO,                     tr("Undo"),                         tr("Ctrl+Z"),           SCENE_SCOPE);
    factory(SCENE_REDO,                     tr("Redo"),                         tr("Ctrl+Shift+Z"),     SCENE_SCOPE);
    factory(SCENE_SAVE_LAYOUT,              tr("Save layout"),                  tr("Ctrl+S"),           SCENE_SCOPE);
    factory(SCENE_SAVE_LAYOUT_AS,           tr("Save layout as..."),            tr("Ctrl+Shift+S"),     SCENE_SCOPE);

    //factory(APP_OPEN_FILE,                  tr("Open file"),                    tr("Ctrl+O"),           GLOBAL_SCOPE);
    factory(APP_NEW_LAYOUT,                 tr("New layout"),                   tr("Ctrl+L"),           GLOBAL_SCOPE);
    factory(APP_SYSTEM_SETTINGS,            tr("System settings"),              tr("Ctrl+P"),           GLOBAL_SCOPE);
    factory(APP_FULLSCREEN,                 tr("Fullscreen"),                   tr("Alt+Enter"),        GLOBAL_SCOPE);
    factory(APP_RECORD,                     tr("Screen recording..."),          tr(""),                 GLOBAL_SCOPE | IS_MENU);
    factory.pushParent(APP_RECORD);
        factory(APP_RECORD_START_STOP,      tr("Start/stop screen recording"),  tr("Alt+R"),            GLOBAL_SCOPE);
        factory(APP_RECORD_SETTINGS,        tr("Screen recording settings"),    tr(""),                 GLOBAL_SCOPE);
    factory.popParent();
    factory(APP_EXIT,                       tr("Exit"),                         tr("Alt+F4"),           GLOBAL_SCOPE);

    assert(m_infoByAction.size() == ACTION_COUNT);
}

QnMenuWrapper::~QnMenuWrapper() {
    return;
}
