#include "action_manager.h"

#include <cassert>

#include <QtGui/QAction>
#include <QtGui/QMenu>
#include <QtGui/QGraphicsItem>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <utils/common/scoped_value_rollback.h>
#include <core/resource_managment/resource_criterion.h>
#include <core/resource/resource.h>

#include <ui/workbench/workbench_context.h>
#include <ui/style/skin.h>
#include <ui/style/noptix_style.h>
#include <ui/screen_recording/screen_recorder.h>

#include "action.h"
#include "action_conditions.h"
#include "action_target_provider.h"
#include "action_parameter_types.h"

Q_DECLARE_METATYPE(QnAction *)

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

    QnActionBuilder toolTip(const QString &toolTip) {
        m_action->setToolTip(toolTip);

        return *this;
    }

    QnActionBuilder toolTipFormat(const QString &toolTipFormat) {
        m_action->setToolTipFormat(toolTipFormat);

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

    QnActionBuilder forbiddenPermissions(Qn::Permissions permissions) {
        m_action->setForbiddenPermissions(permissions);

        return *this;
    }

    QnActionBuilder forbiddenPermissions(const QString &key, Qn::Permissions permissions) {
        m_action->setForbiddenPermissions(key, permissions);

        return *this;
    }

    QnActionBuilder separator(bool isSeparator = true) {
        m_action->setSeparator(isSeparator);
        m_action->setFlags(m_action->flags() | Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::WidgetTarget | Qn::ResourceTarget | Qn::LayoutItemTarget);

        return *this;
    }

    QnActionBuilder conditionalText(const QString &text, QnActionCondition *condition){
        m_action->addConditionalText(condition, text);
        return *this;
    }

    QnActionBuilder conditionalText(const QString &text, const QnResourceCriterion &criterion, Qn::MatchMode matchMode = Qn::All){
        m_action->addConditionalText(new QnResourceActionCondition(criterion, matchMode, m_manager), text);
        return *this;
    }

    QnActionBuilder checkable(bool isCheckable = true){
        m_action->setCheckable(isCheckable);

        return *this;
    }

    QnActionBuilder checked(bool isChecked = true){
        m_action->setChecked(isChecked);

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
    QnActionManager *m_manager;
    QnAction *m_action;
};


// -------------------------------------------------------------------------- //
// QnActionFactory 
// -------------------------------------------------------------------------- //
class QnActionFactory {
public:
    QnActionFactory(QnActionManager *menu, QnAction *parent): 
        m_manager(menu),
        m_lastFreeActionId(Qn::ActionCount),
        m_currentGroup(0)
    {
        m_actionStack.push_back(parent);
        m_lastAction = parent;
    }

    void beginSubMenu() {
        m_actionStack.push_back(m_lastAction);
    }

    void endSubMenu() {
        m_actionStack.pop_back();
    }

    void beginGroup() {
        m_currentGroup = new QActionGroup(m_manager);
    }

    void endGroup() {
        m_currentGroup = NULL;
    }

    QnActionBuilder operator()(Qn::ActionId id) {
        QnAction *action = m_manager->action(id);
        if(action == NULL) {
            action = new QnAction(id, m_manager);
            m_manager->registerAction(action);
        }

        m_actionStack.back()->addChild(action);
        m_lastAction = action;
        if (m_currentGroup)
            m_currentGroup->addAction(action);

        return QnActionBuilder(m_manager, action);
    }

    QnActionBuilder operator()() {
        return operator()(static_cast<Qn::ActionId>(m_lastFreeActionId++));
    }

private:
    QnActionManager *m_manager;
    int m_lastFreeActionId;
    QnAction *m_lastAction;
    QList<QnAction *> m_actionStack;
    QActionGroup* m_currentGroup;
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
        Qn::ActionParameterType type = QnActionParameterTypes::type(items);
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
    m_root(NULL),
    m_targetProvider(NULL),
    m_shortcutAction(NULL),
    m_lastShownMenu(NULL)
{
    m_root = new QnAction(Qn::NoAction, this);
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
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/main_menu.png"));

    factory(Qn::ConnectToServerAction).
        flags(Qn::Main).
        text(tr("Connect to Server...")).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/connected.png"));

    factory(Qn::TogglePanicModeAction).
        flags(Qn::Main).
        text(tr("Start Panic Recording")).
        toggledText(tr("Stop Panic Recording")).
        autoRepeat(false).
        shortcut(tr("Ctrl+P")).
        icon(qnSkin->icon("titlebar/panic.png")).
        //requiredPermissions(Qn::AllMediaServersParameter, Qn::ReadWriteSavePermission).
        condition(new QnPanicActionCondition(this)); // TODO: #gdm disable condition? ask Elric

    factory().
        flags(Qn::Main | Qn::Tree).
        separator();

    factory().
        flags(Qn::Main | Qn::TabBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("New..."));

    factory.beginSubMenu(); {
        factory(Qn::NewUserLayoutAction).
            flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
            requiredPermissions(Qn::CreateLayoutPermission).
            text(tr("Layout...")).
            pulledText(tr("New Layout...")).
            condition(hasFlags(QnResource::user));

        factory(Qn::OpenNewTabAction).
            flags(Qn::Main | Qn::TabBar | Qn::SingleTarget | Qn::NoTarget).
            text(tr("Tab")).
            pulledText(tr("New Tab")).
            shortcut(tr("Ctrl+T")).
            autoRepeat(false). /* Technically, it should be auto-repeatable, but we don't want the user opening 100500 layouts and crashing the client =). */
            icon(qnSkin->icon("titlebar/new_layout.png"));

        factory(Qn::OpenNewWindowAction).
            flags(Qn::Main).
            text(tr("Window")).
            pulledText(tr("New Window")).
            shortcut(tr("Ctrl+N")).
            autoRepeat(false);

        factory(Qn::NewUserAction).
            flags(Qn::Main | Qn::Tree | Qn::NoTarget).
            requiredPermissions(Qn::CurrentUserParameter, Qn::GlobalEditUsersPermission).
            text(tr("User...")).
            pulledText(tr("New User..."));
    } factory.endSubMenu();

    factory().
        flags(Qn::Main | Qn::Scene).
        text(tr("Open..."));

    factory.beginSubMenu(); {
        factory(Qn::OpenFileAction).
            flags(Qn::Main | Qn::Scene).
            requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission | Qn::AddRemoveItemsPermission).
            text(tr("File(s)...")).
            shortcut(tr("Ctrl+O")).
            autoRepeat(false).
            icon(qnSkin->icon("folder.png"));

        factory(Qn::OpenLayoutAction).
            //flags(Qn::Main | Qn::Scene). // TODO
            forbiddenPermissions(Qn::CurrentLayoutParameter, Qn::AddRemoveItemsPermission).
            text(tr("Layout(s)...")).
            autoRepeat(false);

        factory(Qn::OpenFolderAction).
            flags(Qn::Main | Qn::Scene).
            requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission | Qn::AddRemoveItemsPermission).
            text(tr("Folder..."));
    } factory.endSubMenu();

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


    if (QnScreenRecorder::isSupported()){
        factory(Qn::ToggleScreenRecordingAction).
            flags(Qn::Main).
            text(tr("Start Screen Recording")).
            toggledText(tr("Stop Screen Recording")).
            shortcut(tr("Alt+R")).
            shortcut(Qt::Key_MediaRecord).
            shortcutContext(Qt::ApplicationShortcut).
            autoRepeat(false);
    }

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
        icon(qnSkin->icon("titlebar/fullscreen.png", "titlebar/unfullscreen.png"));

    registerAlias(Qn::EffectiveMaximizeAction, Qn::FullscreenAction);

    factory(Qn::MinimizeAction).
        flags(Qn::NoTarget).
        text(tr("Minimize")).
        icon(qnSkin->icon("titlebar/minimize.png"));

    factory(Qn::MaximizeAction).
        flags(Qn::Main).
        text(tr("Maximize")).
        toggledText(tr("Restore Down")).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/fullscreen.png", "titlebar/unfullscreen.png")); // TODO: icon?

    factory(Qn::SystemSettingsAction).
        flags(Qn::Main).
        text(tr("System Settings...")).
        //shortcut(tr("Ctrl+P")).
        role(QAction::PreferencesRole).
        autoRepeat(false);

    factory().
        flags(Qn::Main).
        separator();
    
    factory(Qn::AboutAction).
        flags(Qn::Main).
        text(tr("About...")).
        shortcut(tr("F1")).
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
        icon(qnSkin->icon("titlebar/exit.png"));


    /* Tab bar actions. */
    factory().
        flags(Qn::TabBar).
        separator();

    factory(Qn::CloseLayoutAction).
        flags(Qn::TabBar | Qn::ScopelessHotkey | Qn::SingleTarget).
        text(tr("Close")).
        shortcut(tr("Ctrl+W")).
        autoRepeat(false);

    factory(Qn::CloseAllButThisLayoutAction).
        flags(Qn::TabBar | Qn::SingleTarget).
        text(tr("Close All But This")).
        condition(new QnLayoutCountActionCondition(2, this));



    /* Resource actions. */
    factory(Qn::OpenInLayoutAction).
        flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredPermissions(Qn::LayoutParameter, Qn::WritePermission | Qn::AddRemoveItemsPermission).
        text(tr("Open in Layout"));

    factory(Qn::OpenInCurrentLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission | Qn::AddRemoveItemsPermission).
        text(tr("Open")).
        conditionalText(tr("Monitor"), hasFlags(QnResource::server), Qn::All).
        condition(hasFlags(QnResource::media) || hasFlags(QnResource::server), Qn::Any);

    factory(Qn::OpenInNewLayoutAction).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in a New Tab")).
        conditionalText(tr("Monitor in a New Tab"), hasFlags(QnResource::server), Qn::All).
        condition(hasFlags(QnResource::media) || hasFlags(QnResource::server), Qn::Any);

    factory(Qn::OpenInNewWindowAction).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in a New Window")).
        conditionalText(tr("Monitor in a New Window"), hasFlags(QnResource::server), Qn::All).
        condition(hasFlags(QnResource::media) || hasFlags(QnResource::server), Qn::Any);

    factory(Qn::OpenInFolderAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Open Containing Folder")).
        shortcut(tr("Ctrl+Enter")).
        shortcut(tr("Ctrl+Return")).
        autoRepeat(false).
        condition(hasFlags(QnResource::url | QnResource::local | QnResource::media));

    factory(Qn::OpenSingleLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Open Layout in a new Tab")).
        condition(hasFlags(QnResource::layout));

    factory(Qn::OpenMultipleLayoutsAction).
        flags(Qn::Tree | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layouts")).
        condition(hasFlags(QnResource::layout));

    factory(Qn::OpenNewWindowLayoutsAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layout(s) in a new Window")).
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

    factory(Qn::StartSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Show Motion/Smart Search")).
        shortcut(tr("Alt+G")).
        condition(new QnSmartSearchActionCondition(false, this));

    factory(Qn::StopSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Hide Motion/Smart Search")).
        shortcut(tr("Alt+G")).
        condition(new QnSmartSearchActionCondition(true, this));

    factory(Qn::ClearMotionSelectionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Clear Motion Selection")).
        condition(new QnClearMotionSelectionActionCondition(this));

    factory(Qn::ToggleSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::HotkeyOnly).
        text(tr("Toggle Smart Search")).
        shortcut(tr("Alt+G")).
        condition(new QnSmartSearchActionCondition(this));

    factory(Qn::CheckFileSignatureAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Check File Watermark")).
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
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
         text(tr("Rotate to..."));

    factory.beginSubMenu();{
        factory(Qn::Rotate0Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("0 degrees"));

        factory(Qn::Rotate90Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("90 degrees"));

        factory(Qn::Rotate180Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("180 degrees"));

        factory(Qn::Rotate270Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("270 degrees"));
    } factory.endSubMenu();

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
        //flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget). // TODO
        text(tr("Upload to YouTube...")).
        //shortcut(tr("Ctrl+Y")).
        autoRepeat(false).
        condition(hasFlags(QnResource::ARCHIVE));

    factory(Qn::EditTagsAction).
        //flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget). // TODO
        text(tr("Edit tags...")).
        shortcut(tr("Alt+T")).
        autoRepeat(false).
        condition(hasFlags(QnResource::media));

    factory(Qn::DeleteFromDiskAction).
        //flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget). // TODO
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
        requiredPermissions(Qn::WritePermission).
        condition(new QnResourceActionCondition(hasFlags(QnResource::live_cam), Qn::Any, this));

    factory(Qn::OpenInCameraSettingsDialogAction).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in Camera Settings Dialog"));

    factory(Qn::ServerAddCameraManuallyAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Add camera(s)...")).
        condition(new QnResourceActionCondition(hasFlags(QnResource::remote_server), Qn::ExactlyOne, this));

    factory(Qn::ServerSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Settings...")).
        requiredPermissions(Qn::WritePermission).
        condition(new QnResourceActionCondition(hasFlags(QnResource::remote_server), Qn::ExactlyOne, this));

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Change Cell Aspect Ratio"));

    factory.beginSubMenu(); {
        factory.beginGroup();

        factory(Qn::SetCurrentLayoutAspectRatio4x3Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission).
            text(tr("4:3")).
            checkable();

        factory(Qn::SetCurrentLayoutAspectRatio16x9Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission).
            text(tr("16:9")).
            checkable().
            checked(); // TODO: #gdm - runtime check of DEFAULT_LAYOUT_CELL_ASPECT_RATIO ?

        factory.endGroup();
    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Change Cell Spacing"));

    factory.beginSubMenu(); {
        factory.beginGroup();

        factory(Qn::SetCurrentLayoutItemSpacing0Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission).
            text(tr("None")).
            checkable();

        factory(Qn::SetCurrentLayoutItemSpacing10Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission).
            text(tr("Small")).
            checkable().
            checked(); // TODO: #gdm - runtime check of DEFAULT_LAYOUT_CELL_SPACING ?

        factory(Qn::SetCurrentLayoutItemSpacing20Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission).
            text(tr("Medium")).
            checkable();

        factory(Qn::SetCurrentLayoutItemSpacing30Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutParameter, Qn::WritePermission).
            text(tr("Large")).
            checkable();
        factory.endGroup();

    } factory.endSubMenu();

    factory(Qn::ToggleTourModeAction).
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Start Tour")).
        toggledText(tr("Stop Tour")).
        condition(new QnToggleTourActionCondition(this));

    factory(Qn::StartTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Mark Selection Start")).
        shortcut(tr("[")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new QnTimePeriodActionCondition(Qn::NullTimePeriod, Qn::InvisibleAction, false, this));

    factory(Qn::EndTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Mark Selection End")).
        shortcut(tr("]")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new QnTimePeriodActionCondition(Qn::EmptyTimePeriod, Qn::InvisibleAction, false, this));

    factory(Qn::ClearTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Clear Selection")).
        condition(new QnTimePeriodActionCondition(Qn::EmptyTimePeriod | Qn::NormalTimePeriod, Qn::InvisibleAction, false, this));

    factory(Qn::ExportTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Export Selection...")).
        condition(new QnExportActionCondition(true, this));

    factory(Qn::ExportLayoutAction).
        flags(Qn::Slider | Qn::SingleTarget | Qn::MultiTarget | Qn::NoTarget). 
        text(tr("Export Selection as Multi-Stream...")).
        //condition(new QnTimePeriodActionCondition(Qn::NormalTimePeriod, Qn::DisabledAction, false, this));
        condition(new QnExportActionCondition(false, this));

    factory(Qn::ThumbnailsSearchAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Thumbnails Search...")).
        condition(new QnExportActionCondition(true, this));

    factory().
        flags(Qn::Slider).
        separator();

    factory(Qn::ToggleThumbnailsAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Show Thumbnails")).
        toggledText(tr("Hide Thumbnails"));

    factory(Qn::IncrementDebugCounterAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::NoTarget).
        shortcut(tr("Ctrl+Alt+Shift++")).
        text(tr("Increment Debug Counter"));

    factory(Qn::DecrementDebugCounterAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::NoTarget).
        shortcut(tr("Ctrl+Alt+Shift+-")).
        text(tr("Decrement Debug Counter"));



    factory(Qn::PlayPauseAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("Space")).
        text(tr("Play")).
        toggledText(tr("Pause")).
        condition(new QnArchiveActionCondition(this));

    factory(Qn::SpeedDownAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("Ctrl+-")).
        text(tr("Speed Down")).
        condition(new QnArchiveActionCondition(this));

    factory(Qn::SpeedUpAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("Ctrl++")).
        text(tr("Speed Up")).
        condition(new QnArchiveActionCondition(this));

    factory(Qn::PreviousFrameAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("Ctrl+Left")).
        text(tr("Previous Frame")).
        condition(new QnArchiveActionCondition(this));

    factory(Qn::NextFrameAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("Ctrl+Right")).
        text(tr("Next Frame")).
        condition(new QnArchiveActionCondition(this));

    factory(Qn::JumpToStartAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("Z")).
        text(tr("To Start")).
        condition(new QnArchiveActionCondition(this));

    factory(Qn::JumpToEndAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("X")).
        text(tr("To End")).
        condition(new QnArchiveActionCondition(this));

    factory(Qn::VolumeUpAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("Ctrl+Up")).
        text(tr("Volume Down"));

    factory(Qn::VolumeDownAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("Ctrl+Down")).
        text(tr("Volume Up"));

    factory(Qn::ToggleMuteAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("M")).
        text(tr("Toggle Mute")).
        checkable();

    factory(Qn::JumpToLiveAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("L")).
        text(tr("Jump to Live")).
        checkable().
        condition(new QnArchiveActionCondition(this));

    factory(Qn::ToggleSyncAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(tr("S")).
        text(tr("Synchronize Streams")).
        toggledText(tr("Disable Stream Synchronization")).
        condition(new QnArchiveActionCondition(this));

    factory(Qn::ToggleCalendarAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Show Calendar")).
        toggledText(tr("Hide Calendar"));
}

QnActionManager::~QnActionManager() {
    qDeleteAll(m_idByAction.keys());
}

void QnActionManager::setTargetProvider(QnActionTargetProvider *targetProvider) {
    m_targetProvider = targetProvider;
    m_targetProviderGuard = dynamic_cast<QObject *>(targetProvider);
    if(!m_targetProviderGuard)
        m_targetProviderGuard = this;
}

void QnActionManager::registerAction(QnAction *action) {
    if(!action) {
        qnNullWarning(action);
        return;
    }

    if(m_idByAction.contains(action))
        return; /* Re-registration is allowed. */

    if(m_actionById.contains(action->id())) {
        qnWarning("Action with id '%1' is already registered with this action manager.", action->id());
        return;
    }

    m_actionById[action->id()] = action;
    m_idByAction[action] = action->id();
}

void QnActionManager::registerAlias(Qn::ActionId id, Qn::ActionId targetId) {
    if(id == targetId) {
        qnWarning("Action cannot be an alias of itself.");
        return;
    }

    QnAction *action = this->action(id);
    if(action && action->id() == id) { /* Note that re-registration with different target is OK. */
        qnWarning("Id '%1' is already taken by non-alias action '%2'.", id, action->text());
        return;
    }

    QnAction *targetAction = this->action(targetId);
    if(!targetAction) {
        qnWarning("Action with id '%1' is not registered with this action manager.", targetId);
        return;
    }

    m_actionById[id] = targetAction;
}

QnAction *QnActionManager::action(Qn::ActionId id) const {
    return m_actionById.value(id, NULL);
}

QList<QnAction *> QnActionManager::actions() const {
    return m_idByAction.keys();
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
    foreach(const QByteArray &name, src->dynamicPropertyNames())
        dst->setProperty(name.data(), src->property(name.data()));
    
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

        if (action->hasConditionalTexts())
            newAction->setText(action->checkConditionalText(parameters));

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

void QnActionManager::redirectAction(QMenu *menu, Qn::ActionId sourceId, QAction *targetAction) {
    redirectActionRecursive(menu, sourceId, targetAction);
}

bool QnActionManager::redirectActionRecursive(QMenu *menu, Qn::ActionId sourceId, QAction *targetAction) {
    QList<QAction *> actions = menu->actions();

    foreach(QAction *action, actions) {
        QnAction *storedAction = qnAction(action);
        if(storedAction && storedAction->id() == sourceId) {
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
            bool success = redirectActionRecursive(action->menu(), sourceId, targetAction);
            
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
