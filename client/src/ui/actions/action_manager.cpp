#include "action_manager.h"
#include <cassert>
#include <QAction>
#include <QMenu>
#include <QGraphicsItem>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <core/resourcemanagment/resource_criterion.h>
#include <core/resource/resource.h>
#include <ui/style/skin.h>
#include <ui/style/app_style.h>
#include "action.h"
#include "action_conditions.h"
#include "action_target_provider.h"
#include "action_meta_types.h"

// -------------------------------------------------------------------------- //
// QnActionBuilder 
// -------------------------------------------------------------------------- //
class QnActionBuilder {
public:
    QnActionBuilder(QnActionManager *manager, QnAction *action): m_manager(manager), m_action(action) {}

    QnActionBuilder shortcut(const QKeySequence &shortcut, Qt::ShortcutContext context = Qt::ApplicationShortcut) {
        QList<QKeySequence> shortcuts = m_action->shortcuts();
        shortcuts.push_back(shortcut);
        m_action->setShortcuts(shortcuts);
        m_action->setShortcutContext(context);

        return *this;
    }

    QnActionBuilder icon(const QIcon &icon) {
        m_action->setIcon(icon);

        return *this;
    }

    QnActionBuilder toggledIcon(const QIcon &toggledIcon) {
        QIcon icon = m_action->icon();
        
        for(int i = 0; i <= QIcon::Selected; i++) {
            QIcon::Mode mode = static_cast<QIcon::Mode>(i);
            QIcon::State state = QIcon::On;

            icon.addPixmap(toggledIcon.pixmap(toggledIcon.actualSize(QSize(1024, 1024), mode, state), mode, state), mode, state);
        }

        m_action->setIcon(icon);
        m_action->setCheckable(true);

        return *this;
    }

    QnActionBuilder role(QAction::MenuRole role) {
        m_action->setMenuRole(role);

        return *this;
    }

    QnActionBuilder autoRepeat(bool autoRepeat) {
        m_action->setAutoRepeat(autoRepeat);

        return *this;
    }

    QnActionBuilder text(const QString &text) {
        m_action->setText(text);
        m_action->setNormalText(text);

        return *this;
    }

    QnActionBuilder toggledText(const QString &text) {
        m_action->setToggledText(text);
        m_action->setCheckable(true);

        showCheckBoxInMenu(false);

        return *this;
    }

    QnActionBuilder flags(Qn::ActionFlags flags) {
        m_action->setFlags(m_action->flags() | flags);

        return *this;
    }

    QnActionBuilder separator(bool isSeparator = true) {
        m_action->setSeparator(isSeparator);

        return *this;
    }

    void showCheckBoxInMenu(bool show) {
        m_action->setProperty(hideCheckBoxInMenuPropertyName, !show);
    }

    void condition(QnActionCondition *condition) {
        assert(m_action->condition() == NULL);

        condition->setParent(m_action);
        m_action->setCondition(condition);
    }

private:
    QnAction *m_action;
    QnActionManager *m_manager;
};


// -------------------------------------------------------------------------- //
// QnActionFactory 
// -------------------------------------------------------------------------- //
class QnActionFactory {
public:
    QnActionFactory(QnActionManager *menu, QnAction *parent): 
        m_manager(menu)
    {
        m_actionStack.push_back(parent);
        m_lastAction = parent;
    }

    void enterSubMenu() {
        m_actionStack.push_back(m_lastAction);
    }

    void leaveSubMenu() {
        m_actionStack.pop_back();
    }

    QnActionBuilder operator()(Qn::ActionId id) {
        QnAction *action = m_manager->m_actionById.value(id);
        if(action == NULL) {
            action = new QnAction(m_manager, id, m_manager);
            m_manager->m_actionById[id] = action;
        }

        m_actionStack.back()->addChild(action);
        m_lastAction = action;

        return QnActionBuilder(m_manager, action);
    }

private:
    QnActionManager *m_manager;
    QnAction *m_lastAction;
    QList<QnAction *> m_actionStack;
};


// -------------------------------------------------------------------------- //
// QnActionManager 
// -------------------------------------------------------------------------- //
QnActionManager::QnActionManager(QObject *parent): 
    QObject(parent),
    m_shortcutAction(NULL),
    m_targetProvider(NULL),
    m_root(NULL)
{
    m_root = new QnAction(this, Qn::NoAction, this);
    m_actionById[Qn::NoAction] = m_root;

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
        icon(Skin::icon(QLatin1String("logo_icon2.png")));

    factory(Qn::OpenFileAction).
        flags(Qn::Main).
        text(tr("Open File(s)...")).
        shortcut(tr("Ctrl+O")).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("folder.png")));

    factory(Qn::ScreenRecordingMenu).
        flags(Qn::Main).
        text(tr("Screen Recording"));

    factory.enterSubMenu(); {
        factory(Qn::ScreenRecordingAction).
            flags(Qn::Main).
            text(tr("Start Screen Recording")).
            toggledText(tr("Stop Screen Recording")).
            shortcut(tr("Alt+R")).
            shortcut(Qt::Key_MediaRecord).
            autoRepeat(false);

        factory(Qn::ScreenRecordingSettingsAction).
            flags(Qn::Main).
            text(tr("Screen Recording Settings")).
            autoRepeat(false);
    } factory.leaveSubMenu();

    factory(Qn::FullscreenAction).
        flags(Qn::Main).
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
        flags(Qn::Main).
        text(tr("Preferences")).
        shortcut(tr("Ctrl+P")).
        role(QAction::PreferencesRole).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("decorations/settings.png")));

    factory(Qn::ExitSeparator).
        flags(Qn::Main).
        separator();

    factory(Qn::ExitAction).
        flags(Qn::Main).
        text(tr("Exit")).
        shortcut(tr("Alt+F4")).
        role(QAction::QuitRole).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("decorations/exit-application.png")));



    /* Resource actions. */

    factory(Qn::ShowMotionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Show Motion Grid")).
        condition(new QnMotionGridDisplayActionCondition(QnMotionGridDisplayActionCondition::HasHiddenGrid));

    factory(Qn::HideMotionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Hide Motion Grid")).
        condition(new QnMotionGridDisplayActionCondition(QnMotionGridDisplayActionCondition::HasShownGrid));

    // TODO: add CLDeviceSettingsDlgFactory::canCreateDlg(resource) ?
    factory(Qn::CameraSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget).
        text(tr("Camera Settings")).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, new QnResourceCriterion(QnResource::live_cam)));

    factory(Qn::MultipleCameraSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::MultiTarget).
        text(tr("Multiple Camera Settings")).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, new QnResourceCriterion(QnResource::live_cam)));

    factory(Qn::ServerSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget).
        text(tr("Server Settings")).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, new QnResourceCriterion(QnResource::remote_server)));

    factory(Qn::YouTubeUploadAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget).
        text(tr("Upload to YouTube...")).
        shortcut(tr("Ctrl+Y")).
        autoRepeat(false).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, new QnResourceCriterion(QnResource::ARCHIVE)));

    factory(Qn::EditTagsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget).
        text(tr("Edit tags...")).
        shortcut(tr("Alt+T")).
        autoRepeat(false).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, new QnResourceCriterion(QnResource::media)));

    factory(Qn::OpenInFolderAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget).
        text(tr("Open in Containing Folder")).
        shortcut(tr("Ctrl+Enter")).
        autoRepeat(false).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, new QnResourceCriterion(QnResource::url | QnResource::local | QnResource::media)));


    /* Layout actions. */

    factory(Qn::ItemSeparator).
        flags(Qn::Scene).
        separator();

    factory(Qn::RemoveLayoutItemAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Remove from Layout")).
        shortcut(tr("Del")).
        autoRepeat(false);




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

QnActionManager::~QnActionManager() {
    qDeleteAll(m_actionById);
}

Q_GLOBAL_STATIC(QnActionManager, qn_contextMenu);

QnActionManager *QnActionManager::instance() {
    return qn_contextMenu();
}

void QnActionManager::setTargetProvider(QnActionTargetProvider *targetProvider) {
    m_targetProvider = targetProvider;
    m_targetProviderGuard = dynamic_cast<QObject *>(targetProvider);
    if(!m_targetProviderGuard)
        m_targetProviderGuard = this;
}

QAction *QnActionManager::action(Qn::ActionId id) const {
    return m_actionById.value(id, NULL);
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope) {
    return newMenuInternal(m_root, scope, QVariant::fromValue(QnResourceList()));
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QnResourceList &resources) {
    return newMenuInternal(m_root, scope, QVariant::fromValue(resources));
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QList<QGraphicsItem *> &items) {
    return newMenu(scope, QnActionMetaTypes::widgets(items));
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QnResourceWidgetList &widgets) {
    return newMenuInternal(m_root, scope, QVariant::fromValue(widgets));
}

QMenu *QnActionManager::newMenuInternal(const QnAction *parent, Qn::ActionScope scope, const QVariant &items) {
    if(!m_menus.isEmpty())
        qnWarning("New menu was requested even though the previous one wasn't destroyed. Getting action cause will fail.");

    m_lastTarget = items;

    QMenu *result = newMenuRecursive(parent, scope, items);
    
    m_menus.insert(result);
    connect(result, SIGNAL(destroyed(QObject *)), this, SLOT(at_menu_destroyed(QObject *)));
    
    return result;
}

QMenu *QnActionManager::newMenuRecursive(const QnAction *parent, Qn::ActionScope scope, const QVariant &items) {
    QMenu *result = new QMenu();

    foreach(QnAction *action, parent->children()) {
        QAction *newAction = NULL;

        if(!action->satisfiesCondition(scope, items))
            continue;
        
        if(action->children().size() > 0) {
            QMenu *menu = newMenuRecursive(action, scope, items);
            if(menu->isEmpty()) {
                delete menu;
            } else {
                connect(result, SIGNAL(destroyed()), menu, SLOT(deleteLater()));
                newAction = new QAction(result);
                newAction->setMenu(menu);
                newAction->setText(action->text());
                newAction->setIcon(action->icon());
            }
        } else {
            newAction = action;
        }

        if(newAction != NULL)
            result->addAction(newAction);
    }

    return result;
}

QVariant QnActionManager::currentTarget(QnAction *action) const {
    if(m_shortcutAction == action)
        return m_lastTarget;

    if(m_menus.size() == 1)
        return m_lastTarget;

    qnWarning("No active action, no target exists.");
    return QVariant();
}

QnResourceList QnActionManager::currentResourcesTarget(QnAction *action) const {
    return QnActionMetaTypes::resources(currentTarget(action));
}

QnResourceWidgetList QnActionManager::currentWidgetsTarget(QnAction *action) const {
    return QnActionMetaTypes::widgets(currentTarget(action));
}

QVariant QnActionManager::currentTarget(QObject *sender) const {
    QnAction *action = qobject_cast<QnAction *>(sender);
    if(action == NULL) {
        qnWarning("Cause cannot be determined for non-QnAction senders.");
        return QVariant();
    }

    return currentTarget(action);
}

QnResourceList QnActionManager::currentResourcesTarget(QObject *sender) const {
    QnAction *action = qobject_cast<QnAction *>(sender);
    if(action == NULL) {
        qnWarning("Cause cannot be determined for non-QnAction senders.");
        return QnResourceList();
    }

    return currentResourcesTarget(action);
}

QnResourceWidgetList QnActionManager::currentWidgetsTarget(QObject *sender) const {
    QnAction *action = qobject_cast<QnAction *>(sender);
    if(action == NULL) {
        qnWarning("Cause cannot be determined for non-QnAction senders.");
        return QnResourceWidgetList();
    }

    return currentWidgetsTarget(action);
}

void QnActionManager::at_menu_destroyed(QObject *menu) {
    m_menus.remove(menu);
}

