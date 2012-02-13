#include "context_menu.h"
#include <QAction>
#include <QMenu>
#include <cassert>
#include <ui/style/skin.h>
#include <ui/style/app_style.h>
#include "utils/common/checked_cast.h"

namespace {
    const char *actionIdPropertyName = "_qn_actionId";

    Qn::ActionId actionId(QAction *action) {
        QVariant result = action->property(actionIdPropertyName);
        if(result.type() != QVariant::Int)
            return Qn::NoAction;

        return static_cast<Qn::ActionId>(result.value<int>());
    }

    void setActionId(QAction *action, Qn::ActionId id) {
        action->setProperty(actionIdPropertyName, static_cast<int>(id));
    }

} // anonymous namespace



class QnActionBuilder {
public:
    QnActionBuilder(QnContextMenu *menu, QnContextMenu::ActionData *data): m_menu(menu), m_data(data) {}

    QnActionBuilder shortcut(const QKeySequence &shortcut, Qt::ShortcutContext context = Qt::ApplicationShortcut) {
        QList<QKeySequence> shortcuts = m_data->action->shortcuts();
        shortcuts.push_back(shortcut);
        m_data->action->setShortcuts(shortcuts);
        m_data->action->setShortcutContext(context);

        return *this;
    }

    QnActionBuilder icon(const QIcon &icon) {
        m_data->action->setIcon(icon);

        return *this;
    }

    QnActionBuilder toggledIcon(const QIcon &toggledIcon) {
        QIcon icon = m_data->action->icon();
        
        for(int i = 0; i <= QIcon::Selected; i++) {
            QIcon::Mode mode = static_cast<QIcon::Mode>(i);
            QIcon::State state = QIcon::On;

            icon.addPixmap(toggledIcon.pixmap(toggledIcon.actualSize(QSize(1024, 1024), mode, state), mode, state), mode, state);
        }

        m_data->action->setIcon(icon);
        m_data->action->setCheckable(true);

        return *this;
    }

    QnActionBuilder role(QAction::MenuRole role) {
        m_data->action->setMenuRole(role);

        return *this;
    }

    QnActionBuilder autoRepeat(bool autoRepeat) {
        m_data->action->setAutoRepeat(autoRepeat);

        return *this;
    }

    QnActionBuilder text(const QString &text) {
        m_data->action->setText(text);
        m_data->normalText = text;

        return *this;
    }

    QnActionBuilder toggledText(const QString &text) {
        m_data->flags |= QnContextMenu::ToggledText;
        m_data->toggledText = text;
        m_data->action->setCheckable(true);

        QObject::connect(m_data->action, SIGNAL(toggled(bool)), m_menu, SLOT(at_action_toggled()), Qt::UniqueConnection);

        showCheckBoxInMenu(false);

        return *this;
    }

    QnActionBuilder flags(QnContextMenu::ActionFlags flags) {
        m_data->flags |= flags;

        if(flags & QnContextMenu::Separator)
            m_data->action->setSeparator(true);

        return *this;
    }

    void showCheckBoxInMenu(bool show) {
        m_data->action->setProperty(hideCheckBoxInMenuPropertyName, !show);
    }

private:
    QnContextMenu::ActionData *m_data;
    QnContextMenu *m_menu;
};


class QnActionFactory {
public:
    QnActionFactory(QnContextMenu *menu, QnContextMenu::ActionData *parent): 
        m_menu(menu)
    {
        m_dataStack.push_back(parent);
        m_lastData = parent;
    }

    void enterSubMenu() {
        m_dataStack.push_back(m_lastData);
    }

    void leaveSubMenu() {
        m_dataStack.pop_back();
    }

    QnActionBuilder operator()(Qn::ActionId id) {
        QnContextMenu::ActionData *data = m_menu->m_dataById.value(id);
        if(data == NULL) {
            data = new QnContextMenu::ActionData();

            data->id = id;
            data->action = new QAction(m_menu);
            data->action->setIconVisibleInMenu(true);
            setActionId(data->action, id);
            m_menu->m_dataById[id] = data;
        }

        m_dataStack.back()->children.push_back(data);
        m_lastData = data;

        return QnActionBuilder(m_menu, data);
    }

private:
    QnContextMenu *m_menu;
    QnContextMenu::ActionData *m_lastData;
    QList<QnContextMenu::ActionData *> m_dataStack;
};


QnContextMenu::QnContextMenu(QObject *parent): 
    QObject(parent)
{
    m_root = new ActionData();
    m_dataById[Qn::NoAction] = m_root;

    QnActionFactory factory(this, m_root);

    /* Actions that are not assigned to any menu. */
    factory(Qn::AboutAction).
        text(tr("About...")).
        role(QAction::AboutRole).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("info.png")));

    factory(Qn::ConnectionSettingsAction).
        text(tr("Connection Settings")).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("connect.png")));

    factory(Qn::ShowFpsAction).
        text(tr("Show FPS")).
        toggledText(tr("Hide FPS")).
        shortcut(tr("Ctrl+Alt+F")).
        autoRepeat(false);

    factory(Qn::NewTabAction).
        text(tr("New Layout")).
        shortcut(tr("Ctrl+T")).
        autoRepeat(false). /* Technically, it should be auto-repeatable, but we don't want the user opening 100500 layouts and crashing the client =). */
        icon(Skin::icon(QLatin1String("plus.png")));

    factory(Qn::CloseTabAction).
        text(tr("Close Layout")).
        shortcut(tr("Ctrl+W")).
        autoRepeat(true);



    /* Main menu actions. */

    factory(Qn::MainMenuAction).
        text(tr("Main Menu")).
        shortcut(tr("Alt+Space")).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("logo_icon.png")));

    factory(Qn::OpenFileAction).
        flags(Main).
        text(tr("Open File(s)...")).
        shortcut(tr("Ctrl+O")).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("folder.png")));

    factory(Qn::ScreenRecordingMenu).
        flags(Main).
        text(tr("Screen Recording"));

    factory.enterSubMenu(); {
        factory(Qn::ScreenRecordingAction).
            flags(Main).
            text(tr("Start Screen Recording")).
            toggledText(tr("Stop Screen Recording")).
            shortcut(tr("Alt+R")).
            shortcut(Qt::Key_MediaRecord).
            autoRepeat(false);

        factory(Qn::ScreenRecordingSettingsAction).
            flags(Main).
            text(tr("Screen Recording Settings")).
            autoRepeat(false);
    } factory.leaveSubMenu();

    factory(Qn::FullscreenAction).
        flags(Main).
        text(tr("Go to Fullscreen")).
        toggledText(tr("Restore Down")).
        autoRepeat(false).
#ifdef Q_OS_MAC
        shortcut(tr("Ctrl+F")).
#else
        shortcut(tr("Alt+Return")).
        shortcut(tr("Esc")).
#endif
        icon(Skin::icon(QLatin1String("decorations/fullscreen.png"))).
        toggledIcon(Skin::icon(QLatin1String("decorations/unfullscreen.png")));

    factory(Qn::PreferencesAction).
        flags(Main).
        text(tr("Preferences")).
        shortcut(tr("Ctrl+P")).
        role(QAction::PreferencesRole).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("decorations/settings.png")));

    factory(Qn::ExitSeparator).
        flags(Main | Separator);

    factory(Qn::ExitAction).
        flags(Main).
        text(tr("Exit")).
        shortcut(tr("Alt+F4")).
        role(QAction::QuitRole).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("decorations/exit-application.png")));

















#if 0
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
#endif
}

QnContextMenu::~QnContextMenu() {
    qDeleteAll(m_dataById);
}

Q_GLOBAL_STATIC(QnContextMenu, qn_contextMenu);

QnContextMenu *QnContextMenu::instance() {
    return qn_contextMenu();
}

void QnContextMenu::at_action_toggled() {
    QAction *action = checked_cast<QAction *>(sender());
    Qn::ActionId id = actionId(action);
    const ActionData *data = m_dataById[id];

    if(data->flags & ToggledText)
        action->setText(action->isChecked() ? data->toggledText : data->normalText);
}

QAction *QnContextMenu::action(Qn::ActionId id) const {
    return m_dataById[id]->action;
}

QMenu *QnContextMenu::newMenu(Qn::ActionScope scope) const {
    return newMenu(m_root, scope);
}
    
QMenu *QnContextMenu::newMenu(const ActionData *parent, Qn::ActionScope scope) const {
    QMenu *result = new QMenu();

    foreach(const ActionData *data, parent->children) {
        QAction *action = NULL;

        if(!(data->flags & scope))
            continue;

        if(data->children.size() > 0) {
            QMenu *menu = newMenu(data, scope);
            if(menu->isEmpty()) {
                delete menu;
            } else {
                action = new QAction(result);
                action->setMenu(menu);
                action->setText(data->action->text());
                action->setIcon(data->action->icon());
            }
        }

        if(action == NULL)
            action = data->action;

        result->addAction(action);
    }

    return result;
}