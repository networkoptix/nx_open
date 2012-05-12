#include "action_manager.h"
#include <cassert>
#include <QAction>
#include <QMenu>
#include <QGraphicsItem>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <utils/common/scoped_value_rollback.h>
#include <core/resourcemanagment/resource_criterion.h>
#include <core/resource/resource.h>
#include <ui/workbench/workbench_context.h>
#include <ui/style/skin.h>
#include <ui/style/noptix_style.h>
#include "action.h"
#include "action_conditions.h"
#include "action_target_provider.h"
#include "action_target_types.h"

Q_DECLARE_METATYPE(QnAction *);

namespace {
    void copyIconPixmap(const QIcon &src, QIcon::Mode mode, QIcon::State state, QIcon *dst) {
        dst->addPixmap(src.pixmap(src.actualSize(QSize(1024, 1024), mode, state), mode, state), mode, state);
    }

    const char *sourceActionPropertyName = "_qn_sourceAction";

    QnAction *qnAction(QAction *action) {
        QnAction *result = action->property(sourceActionPropertyName).value<QnAction *>();
        if(result)
            return result;

        return checked_cast<QnAction *>(action);
    }

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnActionBuilder 
// -------------------------------------------------------------------------- //
class QnActionBuilder {
public:
    QnActionBuilder(QnActionManager *manager, QnAction *action): 
        m_manager(manager), 
        m_action(action) 
    {
        action->setShortcutContext(Qt::WindowShortcut);
    }

    QnActionBuilder shortcut(const QKeySequence &shortcut) {
        QList<QKeySequence> shortcuts = m_action->shortcuts();
        shortcuts.push_back(shortcut);
        m_action->setShortcuts(shortcuts);

        return *this;
    }

    QnActionBuilder shortcutContext(Qt::ShortcutContext context) {
        m_action->setShortcutContext(context);

        return *this;
    }

    QnActionBuilder icon(const QIcon &icon) {
        m_action->setIcon(icon);

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

    QnActionBuilder pulledText(const QString &text) {
        m_action->setPulledText(text);
        m_action->setFlags(m_action->flags() | Qn::Pullable);

        return *this;
    }

    QnActionBuilder flags(Qn::ActionFlags flags) {
        m_action->setFlags(m_action->flags() | flags);

        return *this;
    }

    QnActionBuilder requiredPermissions(Qn::Permissions permissions) {
        m_action->setRequiredPermissions(permissions);

        return *this;
    }

    QnActionBuilder requiredPermissions(const QString &key, Qn::Permissions permissions) {
        m_action->setRequiredPermissions(key, permissions);

        return *this;
    }

    QnActionBuilder separator(bool isSeparator = true) {
        m_action->setSeparator(isSeparator);
        m_action->setFlags(m_action->flags() | Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::WidgetTarget | Qn::ResourceTarget | Qn::LayoutItemTarget);

        return *this;
    }

    void showCheckBoxInMenu(bool show) {
        m_action->setProperty(Qn::HideCheckBoxInMenu, !show);
    }

    void condition(QnActionCondition *condition) {
        assert(m_action->condition() == NULL);

        m_action->setCondition(condition);
    }

    void condition(const QnResourceCriterion &criterion, Qn::MatchMode matchMode = Qn::All) {
        assert(m_action->condition() == NULL);

        m_action->setCondition(new QnResourceActionCondition(criterion, matchMode, m_manager));
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
        m_manager(menu),
        m_lastFreeActionId(Qn::ActionCount)
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

    QnActionBuilder operator()() {
        return operator()(static_cast<Qn::ActionId>(m_lastFreeActionId++));
    }

private:
    int m_lastFreeActionId;
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
        Qn::ActionTargetType type = QnActionTargetTypes::type(items);
        if(type == 0) {
            qnWarning("Unrecognized action target type '%1'.", items.typeName());
            return false;
        }

        return true;
    }

} // anonymous namespace


QnActionManager::QnActionManager(QObject *parent): 
    QObject(parent),
    QnWorkbenchContextAware(parent),
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

    factory(Qn::ShowFpsAction).
        flags(Qn::NoTarget).
        text(tr("Show FPS")).
        toggledText(tr("Hide FPS")).
        shortcut(tr("Ctrl+Alt+F")).
        autoRepeat(false);

    factory(Qn::DropResourcesAction).
        flags(Qn::ResourceTarget | Qn::WidgetTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Drop Resources"));

    factory(Qn::DropResourcesIntoNewLayoutAction).
        flags(Qn::ResourceTarget | Qn::WidgetTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Drop Resources into a New Layout"));

    factory(Qn::DelayedDropResourcesAction).
        flags(Qn::NoTarget).
        text(tr("Delayed Drop Resources"));

    factory(Qn::MoveCameraAction).
        flags(Qn::ResourceTarget | Qn::SingleTarget | Qn::MultiTarget).
        requiredPermissions(Qn::RemovePermission).
        text(tr("Move Cameras")).
        condition(hasFlags(QnResource::network));

    factory(Qn::NextLayoutAction).
        flags(Qn::NoTarget).
        text(tr("Next Layout")).
        shortcut(tr("Ctrl+Tab")).
        autoRepeat(false);

    factory(Qn::PreviousLayoutAction).
        flags(Qn::NoTarget).
        text(tr("Previous Layout")).
        shortcut(tr("Ctrl+Shift+Tab")).
        autoRepeat(false);

    factory(Qn::SelectAllAction).
        flags(Qn::NoTarget).
        text(tr("Select All")).
        shortcut(tr("Ctrl+A")).
        autoRepeat(false);

    factory(Qn::SelectionChangeAction).
        flags(Qn::NoTarget).
        text(tr("Selection Changed"));

    factory(Qn::GetMoreLicensesAction).
        flags(Qn::NoTarget).
        text(tr("Get More Licenses..."));

    factory(Qn::ReconnectAction).
        flags(Qn::NoTarget).
        text(tr("Reconnect to Server"));

    factory(Qn::FreespaceAction).
        flags(Qn::NoTarget).
        text(tr("Go to Freespace Mode")).
        shortcut(tr("F11")).
        autoRepeat(false);


    /* Context menu actions. */

    factory(Qn::FitInViewAction).
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Fit in View"));

    factory().
        flags(Qn::Scene).
        separator();

    factory(Qn::MainMenuAction).
        flags(Qn::NoTarget).
        text(tr("Main Menu")).
        shortcut(tr("Alt+Space")).
        autoRepeat(false);

    factory(Qn::LightMainMenuAction).
        flags(Qn::NoTarget).
        text(tr("Main Menu")).
        icon(qnSkin->icon("main_menu_fullscreen.png"));

    factory(Qn::DarkMainMenuAction).
        flags(Qn::NoTarget).
        text(tr("Main Menu")).
        icon(qnSkin->icon("main_menu_windowed.png"));

    factory(Qn::ConnectToServerAction).
        flags(Qn::Main).
        text(tr("Connect to Server...")).
        autoRepeat(false).
        icon(qnSkin->icon("connected.png"));

    factory().
        flags(Qn::Main | Qn::Tree).
        separator();

    factory().
        flags(Qn::Main | Qn::TabBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("New..."));

    factory.enterSubMenu(); {
        factory(Qn::NewUserLayoutAction).
            flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
            requiredPermissions(Qn::CreateLayoutPermission).
            text(tr("Layout...")).
            pulledText(tr("New Layout...")).
            condition(hasFlags(QnResource::user));

        factory(Qn::OpenNewTabAction).
            flags(Qn::Main | Qn::TabBar).
            text(tr("Tab")).
            pulledText(tr("New Tab")).
            shortcut(tr("Ctrl+T")).
            autoRepeat(false). /* Technically, it should be auto-repeatable, but we don't want the user opening 100500 layouts and crashing the client =). */
            icon(qnSkin->icon("decorations/new_layout.png"));

        factory(Qn::OpenNewWindowAction).
            flags(Qn::Main).
            text(tr("Window")).
            pulledText(tr("New Window")).
            shortcut(tr("Ctrl+N")).
            autoRepeat(false);

        factory(Qn::NewUserAction).
            flags(Qn::Main | Qn::Tree | Qn::NoTarget).
            requiredPermissions(Qn::CreateUserPermission).
            text(tr("User...")).
            pulledText(tr("New User..."));
    } factory.leaveSubMenu();

    factory().
        flags(Qn::Main | Qn::Scene).
        text(tr("Open..."));

    factory.enterSubMenu(); {
        factory(Qn::OpenFileAction).
            flags(Qn::Main | Qn::Scene).
            requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission).
            text(tr("File(s)...")).
            shortcut(tr("Ctrl+O")).
            autoRepeat(false).
            icon(qnSkin->icon("folder.png"));

        factory(Qn::OpenFolderAction).
            flags(Qn::Main | Qn::Scene).
            requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission).
            text(tr("Folder..."));
    } factory.leaveSubMenu();

    factory(Qn::SaveCurrentLayoutAction).
        flags(Qn::Main | Qn::Scene | Qn::NoTarget).
        requiredPermissions(Qn::CurrentLayoutParameter, Qn::SavePermission).
        text(tr("Save Current Layout")).
        shortcut(tr("Ctrl+S")).
        autoRepeat(false). /* There is no point in saving the same layout many times in a row. */
        condition(new QnSaveLayoutActionCondition(true, this));

    factory(Qn::SaveCurrentLayoutAsAction).
        requiredPermissions(Qn::CurrentUserParameter, Qn::CreateLayoutPermission).
        requiredPermissions(Qn::CurrentLayoutParameter, Qn::SavePermission).
        flags(Qn::Main | Qn::Scene | Qn::NoTarget).
        text(tr("Save Current Layout As...")).
        shortcut(tr("Ctrl+Alt+S")).
        autoRepeat(false);

    factory().
        flags(Qn::Main).
        separator();

    factory(Qn::ScreenRecordingAction).
        flags(Qn::Main).
        text(tr("Start Screen Recording")).
        toggledText(tr("Stop Screen Recording")).
        shortcut(tr("Alt+R")).
        shortcut(Qt::Key_MediaRecord).
        shortcutContext(Qt::ApplicationShortcut).
        icon(qnSkin->icon("decorations/recording.png")).
        autoRepeat(false);

    factory(Qn::FullscreenAction).
        flags(Qn::Main).
        text(tr("Go to Fullscreen")).
        toggledText(tr("Exit Fullscreen")).
        autoRepeat(false).
#ifdef Q_OS_MAC
        shortcut(tr("Ctrl+F")).
#else
        shortcut(tr("Alt+Enter")).
        shortcut(tr("Alt+Return")).
        shortcut(tr("Esc")).
#endif
        icon(qnSkin->icon("decorations/fullscreen.png", "decorations/unfullscreen.png"));

    factory(Qn::MinimizeAction).
        flags(Qn::NoTarget).
        text(tr("Minimize")).
        icon(qnSkin->icon("decorations/minimize.png"));

    factory(Qn::MaximizeAction).
        flags(Qn::NoTarget).
        text(tr("Maximize")).
        icon(qnSkin->icon("decorations/maximize.png"));

    factory(Qn::SystemSettingsAction).
        flags(Qn::Main).
        text(tr("System Settings...")).
        shortcut(tr("Ctrl+P")).
        role(QAction::PreferencesRole).
        autoRepeat(false).
        icon(qnSkin->icon("decorations/settings.png"));

    factory().
        flags(Qn::Main).
        separator();
    
    factory(Qn::AboutAction).
        flags(Qn::Main).
        text(tr("About...")).
        role(QAction::AboutRole).
        autoRepeat(false);

    factory().
        flags(Qn::Main).
        separator();

    factory(Qn::ExitAction).
        flags(Qn::Main).
        text(tr("Exit")).
        shortcut(tr("Alt+F4")).
        shortcutContext(Qt::ApplicationShortcut).
        role(QAction::QuitRole).
        autoRepeat(false).
        icon(qnSkin->icon("decorations/exit.png"));


    /* Tab bar actions. */
    factory().
        flags(Qn::TabBar).
        separator();

    factory(Qn::CloseLayoutAction).
        flags(Qn::TabBar | Qn::ScopelessHotkey).
        text(tr("Close")).
        shortcut(tr("Ctrl+W")).
        autoRepeat(false);

    factory(Qn::CloseAllButThisLayoutAction).
        flags(Qn::TabBar).
        text(tr("Close All But This")).
        condition(new QnLayoutCountActionCondition(2, this));



    /* Resource actions. */
    factory(Qn::OpenInLayoutAction).
        flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredPermissions(Qn::LayoutParameter, Qn::WritePermission).
        text(tr("Open in Layout"));

    factory(Qn::OpenInCurrentLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission).
        text(tr("Open")).
        condition(hasFlags(QnResource::media), Qn::Any);

    factory(Qn::OpenInNewLayoutAction).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in a New Tab")).
        condition(hasFlags(QnResource::media), Qn::Any);

    factory(Qn::OpenInNewWindowAction).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in a New Window")).
        condition(hasFlags(QnResource::media), Qn::Any);

    factory(Qn::OpenInFolderAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Open in Containing Folder")).
        shortcut(tr("Ctrl+Enter")).
        shortcut(tr("Ctrl+Return")).
        autoRepeat(false).
        condition(hasFlags(QnResource::url | QnResource::local | QnResource::media));

    factory(Qn::OpenSingleLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Open Layout")).
        condition(hasFlags(QnResource::layout));

    factory(Qn::OpenMultipleLayoutsAction).
        flags(Qn::Tree | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layouts")).
        condition(hasFlags(QnResource::layout));

    factory(Qn::OpenAnyNumberOfLayoutsAction).
        flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layout(s)")).
        condition(hasFlags(QnResource::layout));


    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();


    factory(Qn::SaveLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::SavePermission).
        text(tr("Save Layout")).
        condition(new QnSaveLayoutActionCondition(false, this));

    factory(Qn::SaveLayoutAsAction).
        flags(Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::SavePermission).
        requiredPermissions(Qn::UserParameter, Qn::CreateLayoutPermission).
        text(tr("Save Layout As...")).
        condition(hasFlags(QnResource::layout));

    factory(Qn::SaveLayoutForCurrentUserAsAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::SavePermission).
        requiredPermissions(Qn::CurrentUserParameter, Qn::CreateLayoutPermission).
        text(tr("Save Layout As...")).
        condition(hasFlags(QnResource::layout));


    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(Qn::MaximizeItemAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Maximize Item")).
        shortcut(tr("Enter")).
        shortcut(tr("Return")).
        autoRepeat(false).
        condition(new QnItemZoomedActionCondition(false, this));

    factory(Qn::UnmaximizeItemAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Restore Item")).
        shortcut(tr("Enter")).
        shortcut(tr("Return")).
        autoRepeat(false).
        condition(new QnItemZoomedActionCondition(true, this));

    factory(Qn::ShowMotionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Show Motion Grid")).
        shortcut(tr("Alt+G")).
        condition(new QnMotionGridDisplayActionCondition(false, this));

    factory(Qn::HideMotionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Hide Motion Grid")).
        shortcut(tr("Alt+G")).
        condition(new QnMotionGridDisplayActionCondition(true, this));

    factory(Qn::ToggleMotionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::HotkeyOnly).
        text(tr("Toggle Motion Grid Display")).
        shortcut(tr("Alt+G")).
        condition(new QnMotionGridDisplayActionCondition(this));

    factory(Qn::CheckFileSignatureAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Check file watermark")).
        shortcut(tr("Alt+C")).
        autoRepeat(false).
        condition(new QnCheckFileSignatureActionCondition(this));

    factory(Qn::TakeScreenshotAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Take Screenshot")).
        shortcut(tr("Alt+S")).
        autoRepeat(false).
        condition(new QnTakeScreenshotActionCondition(this));


    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();


    factory(Qn::RemoveLayoutItemAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::LayoutItemTarget | Qn::IntentionallyAmbiguous).
        text(tr("Remove from Layout")).
        shortcut(tr("Del")).
        autoRepeat(false).
        condition(new QnLayoutItemRemovalActionCondition(this));

    factory(Qn::RemoveFromServerAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::IntentionallyAmbiguous).
        requiredPermissions(Qn::RemovePermission).
        text(tr("Delete")).
        shortcut(tr("Del")).
        autoRepeat(false).
        condition(new QnResourceRemovalActionCondition(this));


    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(Qn::RenameAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::WritePermission).
        text(tr("Rename")).
        shortcut(tr("F2")).
        autoRepeat(false).
        condition(hasFlags(QnResource::layout) || hasFlags(QnResource::media) || hasFlags(QnResource::remote_server));

    factory(Qn::YouTubeUploadAction).
        //flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Upload to YouTube...")).
        //shortcut(tr("Ctrl+Y")).
        autoRepeat(false).
        condition(hasFlags(QnResource::ARCHIVE));

    factory(Qn::EditTagsAction).
        //flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Edit tags...")).
        shortcut(tr("Alt+T")).
        autoRepeat(false).
        condition(hasFlags(QnResource::media));

    factory(Qn::DeleteFromDiskAction).
        //flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Delete from Disk")).
        autoRepeat(false).
        condition(hasFlags(QnResource::url | QnResource::local | QnResource::media));

    factory(Qn::UserSettingsAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("User Settings...")).
        condition(hasFlags(QnResource::user));

    factory(Qn::CameraSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Camera Settings...")).
        condition(new QnResourceActionCondition(hasFlags(QnResource::live_cam), Qn::Any, this));

    factory(Qn::OpenInCameraSettingsDialogAction).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in Camera Settings Dialog"));

    factory(Qn::ServerSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Settings...")).
        condition(new QnResourceActionCondition(hasFlags(QnResource::remote_server), Qn::ExactlyOne, this));


    factory(Qn::StartTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Mark Selection Start")).
        shortcut(tr("[")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new QnTimePeriodActionCondition(Qn::NullPeriod, Qn::InvisibleAction, this));

    factory(Qn::EndTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Mark Selection End")).
        shortcut(tr("]")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new QnTimePeriodActionCondition(Qn::EmptyPeriod, Qn::InvisibleAction, this));

    factory(Qn::ClearTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Clear Selection")).
        condition(new QnTimePeriodActionCondition(Qn::NormalPeriod, Qn::InvisibleAction, this));

    factory(Qn::ExportTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Export Selection")).
        condition(new QnTimePeriodActionCondition(Qn::NormalPeriod, Qn::DisabledAction, this));

    factory(Qn::ExportLayoutAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Export selected media as layout")).
        condition(new QnTimePeriodActionCondition(Qn::NormalPeriod, Qn::DisabledAction, this));
}

QnActionManager::~QnActionManager() {
    qDeleteAll(m_actionById);
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

bool QnActionManager::canTrigger(Qn::ActionId id, const QnActionParameters &parameters) {
    QnAction *action = m_actionById.value(id);
    if(!action)
        return false;
    
    return action->checkCondition(action->scope(), parameters);
}

void QnActionManager::trigger(Qn::ActionId id, const QnActionParameters &parameters) {
    QnAction *action = m_actionById.value(id);
    if(action == NULL) {
        qnWarning("Invalid action id '%1'.", static_cast<int>(id));
        return;
    }

    if(action->checkCondition(action->scope(), parameters) != Qn::EnabledAction) {
        qnWarning("Action '%1' was triggered with a parameter that does not meet the action's requirements.", action->text());
        return;
    }

    QnScopedValueRollback<QnActionParameters> paramsRollback(&m_parametersByMenu[NULL], parameters);
    QnScopedValueRollback<QnAction *> actionRollback(&m_shortcutAction, action);
    action->trigger();
}

QMenu *QnActionManager::newMenu(Qn::ActionScope scope, const QnActionParameters &parameters) {
    QMenu *result = newMenuRecursive(m_root, scope, parameters);
    m_parametersByMenu[result] = parameters;

    connect(result, SIGNAL(destroyed(QObject *)), this, SLOT(at_menu_destroyed(QObject *)));
    connect(result, SIGNAL(aboutToShow()), this, SLOT(at_menu_aboutToShow()));

    return result;
}

void QnActionManager::copyAction(QAction *dst, QnAction *src, bool forwardSignals) {
    dst->setText(src->text());
    dst->setIcon(src->icon());
    dst->setShortcuts(src->shortcuts());
    dst->setCheckable(src->isCheckable());
    dst->setChecked(src->isChecked());
    dst->setFont(src->font());
    dst->setIconText(src->iconText());
    dst->setProperty(sourceActionPropertyName, QVariant::fromValue<QnAction *>(src));
    
    if(forwardSignals) {
        connect(dst, SIGNAL(triggered()),   src, SLOT(trigger()));
        connect(dst, SIGNAL(toggled(bool)), src, SLOT(setChecked(bool)));
    }
}

QMenu *QnActionManager::newMenuRecursive(const QnAction *parent, Qn::ActionScope scope, const QnActionParameters &parameters) {
    QMenu *result = new QMenu();

    foreach(QnAction *action, parent->children()) {
        Qn::ActionVisibility visibility;
        if(action->flags() & Qn::HotkeyOnly) {
            visibility = Qn::InvisibleAction;
        } else {
            visibility = action->checkCondition(scope, parameters);
        }
        if(visibility == Qn::InvisibleAction)
            continue;

        QString replacedText;
        QMenu *menu = NULL;
        if(action->children().size() > 0) {
            menu = newMenuRecursive(action, scope, parameters);
            if(menu->isEmpty()) {
                delete menu;
                menu = NULL;
                visibility = Qn::InvisibleAction;
            } else if(menu->actions().size() == 1) {
                QnAction *menuAction = qnAction(menu->actions()[0]);
                if(menuAction->flags() & Qn::Pullable) {
                    delete menu;
                    menu = NULL;

                    action = menuAction;
                    visibility = action->checkCondition(scope, parameters);
                    replacedText = action->pulledText();
                }
            }

            if(menu)
                connect(result, SIGNAL(destroyed()), menu, SLOT(deleteLater()));
        }

        QAction *newAction = NULL;
        if(!replacedText.isEmpty() || visibility == Qn::DisabledAction || menu != NULL) {
            newAction = new QAction(result);
            copyAction(newAction, action);
            
            newAction->setMenu(menu);
            newAction->setDisabled(visibility == Qn::DisabledAction);
            if(!replacedText.isEmpty())
                newAction->setText(replacedText);
        } else {
            newAction = action;
        }

        if(visibility != Qn::InvisibleAction)
            result->addAction(newAction);
    }

    return result;
}

QnActionParameters QnActionManager::currentParameters(QnAction *action) const {
    if(m_shortcutAction == action)
        return m_parametersByMenu.value(NULL);

    if(m_lastShownMenu == NULL || !m_parametersByMenu.contains(m_lastShownMenu))
        qnWarning("No active menu, no target exists.");

    return m_parametersByMenu.value(m_lastShownMenu);
}

QnActionParameters QnActionManager::currentParameters(QObject *sender) const {
    if(QnAction *action = checkSender(sender)) {
        return currentParameters(action);
    } else {
        return QnActionParameters();
    }
}

void QnActionManager::redirectAction(QMenu *menu, Qn::ActionId targetId, QAction *targetAction) {
    redirectActionRecursive(menu, targetId, targetAction);
}

bool QnActionManager::redirectActionRecursive(QMenu *menu, Qn::ActionId targetId, QAction *targetAction) {
    QList<QAction *> actions = menu->actions();

    foreach(QAction *action, actions) {
        QnAction *storedAction = qnAction(action);
        if(storedAction && storedAction->id() == targetId) {
            int index = actions.indexOf(action);
            QAction *before = index + 1 < actions.size() ? actions[index + 1] : NULL;

            menu->removeAction(action);
            if(targetAction != NULL) {
                copyAction(targetAction, storedAction, false);
                targetAction->setEnabled(action->isEnabled());
                menu->insertAction(before, targetAction);
            }

            return true;
        }

        if(action->menu() != NULL) {
            bool success = redirectActionRecursive(action->menu(), targetId, targetAction);
            
            if(success && action->menu()->isEmpty())
                menu->removeAction(action);

            return success;
        }
    }
        
    return false;
}

void QnActionManager::at_menu_destroyed(QObject *menu) {
    m_parametersByMenu.remove(menu);
}

void QnActionManager::at_menu_aboutToShow() {
    m_lastShownMenu = sender();
}
