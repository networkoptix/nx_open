#include "action_manager.h"
#include <cassert>
#include <QAction>
#include <QMenu>
#include <QGraphicsItem>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <core/resourcemanagment/resource_criterion.h>
#include <core/resource/resource.h>
#include <ui/workbench/workbench_context.h>
#include <ui/style/skin.h>
#include <ui/style/noptix_style.h>
#include "action.h"
#include "action_conditions.h"
#include "action_target_provider.h"
#include "action_target_types.h"

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
        condition->setManager(m_manager);
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
            m_manager->m_idByAction[action] = id;
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
namespace {
    QnAction *checkSender(QObject *sender) {
        QnAction *result = qobject_cast<QnAction *>(sender);
        if(result == NULL)
            qnWarning("Cause cannot be determined for non-QnAction senders.");
        return result;
    }

    bool checkType(const QVariant &items) {
        int type = items.userType();
        if(type != QnActionTargetTypes::resourceList() && type != QnActionTargetTypes::widgetList() && type != QnActionTargetTypes::layoutItemList() && type != QnActionTargetTypes::layoutList()) {
            qnWarning("Unrecognized menu target type '%1'.", items.typeName());
            return false;
        }

        return true;
    }

} // anonymous namespace


QnActionManager::QnActionManager(QObject *parent): 
    QObject(parent),
    m_shortcutAction(NULL),
    m_targetProvider(NULL),
    m_root(NULL),
    m_lastShownMenu(NULL)
{
    m_root = new QnAction(this, Qn::NoAction, this);
    m_actionById[Qn::NoAction] = m_root;
    m_idByAction[m_root] = Qn::NoAction;

    QnActionFactory factory(this, m_root);

    using namespace QnResourceCriterionExpressions;

    /* Actions that are not assigned to any menu. */
    factory(Qn::AboutAction).
        flags(Qn::NoTarget).
        text(tr("About...")).
        role(QAction::AboutRole).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("info.png")));

    factory(Qn::ConnectionSettingsAction).
        flags(Qn::NoTarget).
        text(tr("Connection Settings")).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("connect.png")));

    factory(Qn::ShowFpsAction).
        flags(Qn::NoTarget).
        text(tr("Show FPS")).
        toggledText(tr("Hide FPS")).
        shortcut(tr("Ctrl+Alt+F")).
        autoRepeat(false);

    factory(Qn::ResourceDropAction).
        flags(Qn::Resource | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Drop Resources"));


    /* Main menu actions. */

    factory(Qn::LightMainMenuAction).
        text(tr("Main Menu")).
        shortcut(tr("Alt+Space")).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("logo_icon2.png")));

    factory(Qn::DarkMainMenuAction).
        text(tr("Main Menu")).
        shortcut(tr("Alt+Space")).
        autoRepeat(false).
        icon(Skin::icon(QLatin1String("logo_icon2_dark.png")));

    factory(Qn::OpenNewLayoutAction).
        flags(Qn::Main).
        text(tr("New Layout")).
        shortcut(tr("Ctrl+T")).
        autoRepeat(false). /* Technically, it should be auto-repeatable, but we don't want the user opening 100500 layouts and crashing the client =). */
        icon(Skin::icon(QLatin1String("plus.png")));

    factory(Qn::SaveCurrentLayoutAction).
        flags(Qn::Main | Qn::Scene | Qn::NoTarget).
        text(tr("Save Current Layout")).
        shortcut(tr("Ctrl+S")).
        autoRepeat(false). /* There is no point in saving the same layout many times in a row. */
        condition(new QnResourceSaveLayoutActionCondition(true));

    factory(Qn::SaveCurrentLayoutAsAction).
        flags(Qn::Main | Qn::Scene | Qn::NoTarget).
        text(tr("Save Current Layout As...")).
        shortcut(tr("Ctrl+Alt+S")).
        autoRepeat(false);

    factory(Qn::OpenMenu).
        flags(Qn::Main).
        text(tr("Open"));

    factory.enterSubMenu(); {
        factory(Qn::OpenFileAction).
            flags(Qn::Main).
            text(tr("File(s)...")).
            shortcut(tr("Ctrl+O")).
            autoRepeat(false).
            icon(Skin::icon(QLatin1String("folder.png")));

        factory(Qn::OpenFolderAction).
            flags(Qn::Main).
            text(tr("Folder..."));
    } factory.leaveSubMenu();

    factory(Qn::FileSeparator).
        flags(Qn::Main).
        separator();

    factory(Qn::ScreenRecordingAction).
        flags(Qn::Main).
        text(tr("Start Screen Recording")).
        toggledText(tr("Stop Screen Recording")).
        shortcut(tr("Alt+R")).
        shortcut(Qt::Key_MediaRecord).
        autoRepeat(false);

    factory(Qn::FullscreenAction).
        flags(Qn::Main).
        text(tr("Go to Fullscreen")).
        toggledText(tr("Restore Down")).
        autoRepeat(false).
#ifdef Q_OS_MAC
        shortcut(tr("Ctrl+F")).
#else
        shortcut(tr("Alt+Enter")).
        shortcut(tr("Alt+Return")).
        shortcut(tr("Esc")).
#endif
        icon(Skin::icon(QLatin1String("decorations/fullscreen.png"))).
        toggledIcon(Skin::icon(QLatin1String("decorations/unfullscreen.png")));

    factory(Qn::SystemSettingsAction).
        flags(Qn::Main).
        text(tr("System Settings")).
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



    /* Tab bar actions. */
    factory(Qn::CloseLayoutAction).
        flags(Qn::TabBar | Qn::ScopelessHotkey).
        text(tr("Close")).
        shortcut(tr("Ctrl+W")).
        autoRepeat(false);



    /* Resource actions. */

    factory(Qn::OpenLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::Resource).
        text(tr("Open Layout")).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::layout)));

    factory(Qn::SaveLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::Resource).
        text(tr("Save Layout")).
        condition(new QnResourceSaveLayoutActionCondition(false));

    factory(Qn::SaveLayoutAsAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::Resource).
        text(tr("Save Layout As...")).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::layout)));

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
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::Resource | Qn::LayoutItem).
        text(tr("Camera Settings")).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::live_cam)));

    factory(Qn::MultipleCameraSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::MultiTarget | Qn::Resource | Qn::LayoutItem).
        text(tr("Multiple Camera Settings")).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::live_cam)));

    factory(Qn::ServerSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::Resource | Qn::LayoutItem).
        text(tr("Server Settings")).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::remote_server)));


    factory(Qn::YouTubeUploadAction).
        //flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::Resource | Qn::LayoutItem).
        text(tr("Upload to YouTube...")).
        //shortcut(tr("Ctrl+Y")).
        autoRepeat(false).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::ARCHIVE)));


    factory(Qn::EditTagsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::Resource | Qn::LayoutItem).
        text(tr("Edit tags...")).
        shortcut(tr("Alt+T")).
        autoRepeat(false).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::media)));

    factory(Qn::OpenInFolderAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::Resource | Qn::LayoutItem).
        text(tr("Open in Containing Folder")).
        shortcut(tr("Ctrl+Enter")).
        shortcut(tr("Ctrl+Return")).
        autoRepeat(false).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::url | QnResource::local | QnResource::media)));

    factory(Qn::DeleteFromDiskAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::Resource | Qn::LayoutItem).
        text(tr("Delete from Disk")).
        autoRepeat(false).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::url | QnResource::local | QnResource::media)));

    /* Layout actions. */

    factory(Qn::ItemSeparator).
        flags(Qn::Scene).
        separator();

    factory(Qn::RemoveLayoutItemAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::LayoutItem | Qn::IntentionallyAmbiguous).
        text(tr("Remove from Layout")).
        shortcut(tr("Del")).
        autoRepeat(false);

    factory(Qn::RemoveFromServerAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::Resource | Qn::IntentionallyAmbiguous).
        text(tr("Remove from Server")).
        shortcut(tr("Del")).
        autoRepeat(false).
        condition(new QnResourceRemovalActionCondition());

    factory(Qn::RenameLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::Resource).
        text(tr("Rename")).
        shortcut(tr("F2")).
        autoRepeat(false).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::layout)));

    factory(Qn::NewUserAction).
        flags(Qn::Tree | Qn::NoTarget).
        text(tr("New User...")).
        condition(new QnResourceActionUserAccessCondition(true));

    factory(Qn::NewLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::Resource).
        text(tr("New Layout...")).
        condition(new QnResourceActionCondition(QnResourceActionCondition::AllMatch, hasFlags(QnResource::user)));

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

void QnActionManager::setContext(QnWorkbenchContext *context) {
    m_context = context;
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

QList<QnAction *> QnActionManager::actions() const {
    return m_actionById.values();
}

void QnActionManager::trigger(Qn::ActionId id, const QVariantMap &params) {
    triggerInternal(id, QVariant::fromValue(QnResourceList()), params);
}

void QnActionManager::trigger(Qn::ActionId id, const QVariant &items, const QVariantMap &params) {
    if(checkType(items)) {
        return triggerInternal(id, items, params);
    } else {
        return triggerInternal(id, QVariant::fromValue(QnResourceList()), params);
    }
}

void QnActionManager::trigger(Qn::ActionId id, const QnResourcePtr &resource, const QVariantMap &params) {
    QnResourceList resources;
    resources.push_back(resource);
    trigger(id, resources, params);
}

void QnActionManager::trigger(Qn::ActionId id, const QnResourceList &resources, const QVariantMap &params) {
    triggerInternal(id, QVariant::fromValue(resources), params);
}

void QnActionManager::trigger(Qn::ActionId id, const QList<QGraphicsItem *> &items, const QVariantMap &params) {
    trigger(id, QnActionTargetTypes::widgets(items), params);
}

void QnActionManager::trigger(Qn::ActionId id, const QnResourceWidgetList &widgets, const QVariantMap &params) {
    triggerInternal(id, QVariant::fromValue(widgets), params);
}

void QnActionManager::trigger(Qn::ActionId id, const QnWorkbenchLayoutList &layouts, const QVariantMap &params) {
    triggerInternal(id, QVariant::fromValue(layouts), params);
}

void QnActionManager::trigger(Qn::ActionId id, const QnLayoutItemIndexList &layoutItems, const QVariantMap &params) {
    triggerInternal(id, QVariant::fromValue(layoutItems), params);
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QVariantMap &params) {
    return newMenuInternal(m_root, scope, QVariant::fromValue(QnResourceList()), params);
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QVariant &items, const QVariantMap &params) {
    if(checkType(items)) {
        return newMenuInternal(m_root, scope, items, params);
    } else {
        return newMenuInternal(m_root, scope, QVariant::fromValue(QnResourceList()), params);
    }
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QnResourceList &resources, const QVariantMap &params) {
    return newMenuInternal(m_root, scope, QVariant::fromValue(resources), params);
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QList<QGraphicsItem *> &items, const QVariantMap &params) {
    return newMenu(scope, QnActionTargetTypes::widgets(items), params);
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QnResourceWidgetList &widgets, const QVariantMap &params) {
    return newMenuInternal(m_root, scope, QVariant::fromValue(widgets), params);
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QnWorkbenchLayoutList &layouts, const QVariantMap &params) {
    return newMenuInternal(m_root, scope, QVariant::fromValue(layouts), params);
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QnLayoutItemIndexList &layoutItems, const QVariantMap &params) {
    return newMenuInternal(m_root, scope, QVariant::fromValue(layoutItems), params);
}

void QnActionManager::copyAction(QAction *dst, const QAction *src) {
    dst->setText(src->text());
    dst->setIcon(src->icon());
    dst->setShortcuts(src->shortcuts());
    dst->setCheckable(src->isCheckable());
    dst->setChecked(src->isChecked());
    dst->setFont(src->font());
    dst->setIconText(src->iconText());
    
    connect(dst, SIGNAL(triggered()),   src, SLOT(trigger()));
    connect(dst, SIGNAL(toggled(bool)), src, SLOT(setChecked(bool)));
}

void QnActionManager::triggerInternal(Qn::ActionId id, const QVariant &items, const QVariantMap &params) {
    QnAction *action = m_actionById.value(id);
    if(action == NULL) {
        qnWarning("Invalid action id '%1'.", static_cast<int>(id));
        return;
    }

    if(action->checkCondition(action->scope(), items) != Qn::VisibleAction) {
        qnWarning("Action '%1' was triggered with a parameter that does not meet the action's requirements.", action->text());
        return;
    }

    m_parametersByMenu[NULL] = ActionParameters(items, params);
    m_shortcutAction = action;
    
    action->trigger();

    m_shortcutAction = NULL;
}

QMenu *QnActionManager::newMenuInternal(const QnAction *parent, Qn::ActionScope scope, const QVariant &items, const QVariantMap &params) {
    QMenu *result = newMenuRecursive(parent, scope, items);
    m_parametersByMenu[result] = ActionParameters(items, params);

    connect(result, SIGNAL(destroyed(QObject *)), this, SLOT(at_menu_destroyed(QObject *)));
    connect(result, SIGNAL(aboutToShow()), this, SLOT(at_menu_aboutToShow()));
    
    return result;
}

QMenu *QnActionManager::newMenuRecursive(const QnAction *parent, Qn::ActionScope scope, const QVariant &items) {
    QMenu *result = new QMenu();

    foreach(QnAction *action, parent->children()) {
        Qn::ActionVisibility visibility = action->checkCondition(scope, items);
        if(visibility == Qn::InvisibleAction)
            continue;
        
        QMenu *menu = NULL;
        if(action->children().size() > 0) {
            menu = newMenuRecursive(action, scope, items);
            if(menu->isEmpty()) {
                delete menu;
                visibility = Qn::InvisibleAction;
            } else {
                connect(result, SIGNAL(destroyed()), menu, SLOT(deleteLater()));
            }
        }

        QAction *newAction = NULL;
        if(visibility == Qn::DisabledAction || menu != NULL) {
            newAction = new QAction(result);
            copyAction(newAction, action);
            
            newAction->setMenu(menu);
            newAction->setDisabled(visibility == Qn::DisabledAction);
        } else {
            newAction = action;
        }

        if(visibility != Qn::InvisibleAction)
            result->addAction(newAction);
    }

    return result;
}

QnActionManager::ActionParameters QnActionManager::currentParametersInternal(QnAction *action) const {
    if(m_shortcutAction == action)
        return m_parametersByMenu.value(NULL);

    if(m_lastShownMenu == NULL || !m_parametersByMenu.contains(m_lastShownMenu))
        qnWarning("No active menu, no target exists.");

    return m_parametersByMenu.value(m_lastShownMenu);
}

Qn::ActionTarget QnActionManager::currentTargetType(QnAction *action) const {
    return QnActionTargetTypes::target(currentTarget(action));
}

QVariant QnActionManager::currentParameter(QnAction *action, const QString &name) const {
    return currentParametersInternal(action).params.value(name);
}

QVariant QnActionManager::currentTarget(QnAction *action) const {
    return currentParametersInternal(action).items;
}

QnResourceList QnActionManager::currentResourcesTarget(QnAction *action) const {
    return QnActionTargetTypes::resources(currentTarget(action));
}

QnResourcePtr QnActionManager::currentResourceTarget(QnAction *action) const {
    QnResourceList resources = currentResourcesTarget(action);

    if(resources.size() != 1)
        qnWarning("Invalid number of target resources for action '%1': expected %2, got %3.", action->text(), 1, resources.size());

    return resources.isEmpty() ? QnResourcePtr() : resources.front();
}

QnWorkbenchLayoutList QnActionManager::currentLayoutsTarget(QnAction *action) const {
    return QnActionTargetTypes::layouts(currentTarget(action));
}

QnLayoutItemIndexList QnActionManager::currentLayoutItemsTarget(QnAction *action) const {
    return QnActionTargetTypes::layoutItems(currentTarget(action));
}

QnResourceWidgetList QnActionManager::currentWidgetsTarget(QnAction *action) const {
    return QnActionTargetTypes::widgets(currentTarget(action));
}

Qn::ActionTarget QnActionManager::currentTargetType(QObject *sender) const {
    return QnActionTargetTypes::target(currentTarget(sender));
}

QVariant QnActionManager::currentParameter(QObject *sender, const QString &name) const {
    if(QnAction *action = checkSender(sender)) {
        return currentParameter(action, name);
    } else {
        return QVariant();
    }
}

QVariant QnActionManager::currentTarget(QObject *sender) const {
    if(QnAction *action = checkSender(sender)) {
        return currentTarget(action);
    } else {
        return QVariant();
    }
}

QnResourceList QnActionManager::currentResourcesTarget(QObject *sender) const {
    if(QnAction *action = checkSender(sender)) {
        return currentResourcesTarget(action);
    } else {
        return QnResourceList();
    }
}

QnResourcePtr QnActionManager::currentResourceTarget(QObject *sender) const {
    if(QnAction *action = checkSender(sender)) {
        return currentResourceTarget(action);
    } else {
        return QnResourcePtr();
    }
}

QnLayoutItemIndexList QnActionManager::currentLayoutItemsTarget(QObject *sender) const {
    if(QnAction *action = checkSender(sender)) {
        return currentLayoutItemsTarget(action);
    } else {
        return QnLayoutItemIndexList();
    }
}

QnWorkbenchLayoutList QnActionManager::currentLayoutsTarget(QObject *sender) const {
    if(QnAction *action = checkSender(sender)) {
        return currentLayoutsTarget(action);
    } else {
        return QnWorkbenchLayoutList();
    }
}

QnResourceWidgetList QnActionManager::currentWidgetsTarget(QObject *sender) const {
    if(QnAction *action = checkSender(sender)) {
        return currentWidgetsTarget(action);
    } else {
        return QnResourceWidgetList();
    }
}

void QnActionManager::redirectAction(QMenu *menu, Qn::ActionId targetId, QAction *targetAction) {
    redirectActionRecursive(menu, targetId, targetAction);
}

bool QnActionManager::redirectActionRecursive(QMenu *menu, Qn::ActionId targetId, QAction *targetAction) {
    QList<QAction *> actions = menu->actions();

    foreach(QAction *action, actions) {
        Qn::ActionId id = m_idByAction.value(action, Qn::NoAction);
        if(id == targetId) {
            int index = actions.indexOf(action);
            
            targetAction->setText(action->text());
            targetAction->setShortcuts(action->shortcuts());
            targetAction->setIcon(action->icon());

            menu->removeAction(action);
            menu->insertAction(index == actions.size() - 1 ? NULL : actions[index], targetAction);
            return true;
        }

        if(action->menu() != NULL)
            if(redirectActionRecursive(action->menu(), targetId, targetAction))
                return true;
    }
        
    return false;
}

void QnActionManager::at_menu_destroyed(QObject *menu) {
    m_parametersByMenu.remove(menu);
}

void QnActionManager::at_menu_aboutToShow() {
    m_lastShownMenu = sender();
}
