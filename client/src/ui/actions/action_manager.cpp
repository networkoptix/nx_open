#include "action_manager.h"

#include <cassert>

#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QGraphicsItem>

#include "action.h"
#include "action_factories.h"
#include "action_conditions.h"
#include "action_target_provider.h"
#include "action_parameter_types.h"

#include <client/client_settings.h>

#include <core/resource_management/resource_criterion.h>
#include <core/resource/resource.h>

#include <ui/workbench/workbench_context.h>
#include <ui/style/skin.h>
#include <ui/style/noptix_style.h>
#include <ui/style/globals.h>
#include <ui/screen_recording/screen_recorder.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <utils/common/scoped_value_rollback.h>

namespace {
    void copyIconPixmap(const QIcon &src, QIcon::Mode mode, QIcon::State state, QIcon *dst) {
        dst->addPixmap(src.pixmap(src.actualSize(QSize(1024, 1024), mode, state), mode, state), mode, state);
    }

    const char *sourceActionPropertyName = "_qn_sourceAction";

    QnAction *qnAction(QAction *action) {
        QnAction *result = action->property(sourceActionPropertyName).value<QnAction *>();
        if(result)
            return result;

        return dynamic_cast<QnAction *>(action);
    }

    class QnMenu: public QMenu {
        typedef QMenu base_type;
    public:
        explicit QnMenu(QWidget *parent = 0): base_type(parent) {}

    protected:
        virtual void mousePressEvent(QMouseEvent *event) override {
            /* This prevents the click from propagating to the underlying widget. */
            setAttribute(Qt::WA_NoMouseReplay);
            base_type::mousePressEvent(event);
        }
    };

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnActionBuilder
// -------------------------------------------------------------------------- //
class QnActionBuilder {
public:
    QnActionBuilder(QnAction *action):
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

    QnActionBuilder requiredPermissions(int key, Qn::Permissions permissions) {
        m_action->setRequiredPermissions(key, permissions);

        return *this;
    }

    QnActionBuilder forbiddenPermissions(Qn::Permissions permissions) {
        m_action->setForbiddenPermissions(permissions);

        return *this;
    }

    QnActionBuilder forbiddenPermissions(int key, Qn::Permissions permissions) {
        m_action->setForbiddenPermissions(key, permissions);

        return *this;
    }

    QnActionBuilder separator(bool isSeparator = true) {
        m_action->setSeparator(isSeparator);
        m_action->setFlags(m_action->flags() | Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::WidgetTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::VideoWallItemTarget);

        return *this;
    }

    QnActionBuilder conditionalText(const QString &text, QnActionCondition *condition){
        m_action->addConditionalText(condition, text);
        return *this;
    }

    QnActionBuilder conditionalText(const QString &text, const QnResourceCriterion &criterion, Qn::MatchMode matchMode = Qn::All){
        m_action->addConditionalText(new QnResourceActionCondition(criterion, matchMode, m_action), text);
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

    QnActionBuilder showCheckBoxInMenu(bool show) {
        m_action->setProperty(Qn::HideCheckBoxInMenu, !show);

        return *this;
    }

    QnActionBuilder condition(QnActionCondition *condition) {
        assert(m_action->condition() == NULL);

        m_action->setCondition(condition);

        return *this;
    }

    QnActionBuilder condition(const QnResourceCriterion &criterion, Qn::MatchMode matchMode = Qn::All) {
        assert(m_action->condition() == NULL);

        m_action->setCondition(new QnResourceActionCondition(criterion, matchMode, m_action));

        return *this;
    }

    QnActionBuilder childFactory(QnActionFactory *childFactory) {
        m_action->setChildFactory(childFactory);
        m_action->setFlags(m_action->flags() | Qn::RequiresChildren);

        return *this;
    }

    QnActionBuilder rotationSpeed(qreal rotationSpeed) {
        m_action->setProperty(Qn::ToolButtonCheckedRotationSpeed, rotationSpeed);

        return *this;
    }

private:
    QnAction *m_action;
};


// -------------------------------------------------------------------------- //
// QnMenuFactory
// -------------------------------------------------------------------------- //
class QnMenuFactory {
public:
    QnMenuFactory(QnActionManager *menu, QnAction *parent):
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

        QnAction *parentAction = m_actionStack.back();
        parentAction->addChild(action);
        parentAction->setFlags(parentAction->flags() | Qn::RequiresChildren);

        m_lastAction = action;
        if (m_currentGroup)
            m_currentGroup->addAction(action);

        return QnActionBuilder(action);
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
    m_lastClickedMenu(NULL)
{
    m_root = new QnAction(Qn::NoAction, this);
    m_actionById[Qn::NoAction] = m_root;
    m_idByAction[m_root] = Qn::NoAction;

    QnMenuFactory factory(this, m_root);

    using namespace QnResourceCriterionExpressions;



    /* Actions that are not assigned to any menu. */

    factory(Qn::ShowFpsAction).
        flags(Qn::GlobalHotkey).
        text(tr("Show FPS")).
        toggledText(tr("Hide FPS")).
        shortcut(tr("Ctrl+Alt+F")).
        autoRepeat(false);

    factory(Qn::ShowDebugOverlayAction).
        flags(Qn::GlobalHotkey).
        text(tr("Show Debug")).
        toggledText(tr("Hide Debug")).
        shortcut(tr("Ctrl+Alt+D")).
        autoRepeat(false);

    factory(Qn::DropResourcesAction).
        flags(Qn::ResourceTarget | Qn::WidgetTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Drop Resources"));

    factory(Qn::DropResourcesIntoNewLayoutAction).
        flags(Qn::ResourceTarget | Qn::WidgetTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Drop Resources into a New Layout"));

    factory(Qn::DelayedOpenVideoWallItemAction).
        flags(Qn::NoTarget).
        text(tr("Delayed Open Video Wall"));

    factory(Qn::DelayedDropResourcesAction).
        flags(Qn::NoTarget).
        text(tr("Delayed Drop Resources"));

    factory(Qn::InstantDropResourcesAction).
        flags(Qn::NoTarget).
        text(tr("Instant Drop Resources"));

    factory(Qn::MoveCameraAction).
        flags(Qn::ResourceTarget | Qn::SingleTarget | Qn::MultiTarget).
        requiredPermissions(Qn::RemovePermission).
        text(tr("Move Cameras")).
        condition(hasFlags(Qn::network));

    factory(Qn::NextLayoutAction).
        flags(Qn::GlobalHotkey).
        text(tr("Next Layout")).
        shortcut(tr("Ctrl+Tab")).
        autoRepeat(false);

    factory(Qn::PreviousLayoutAction).
        flags(Qn::GlobalHotkey).
        text(tr("Previous Layout")).
        shortcut(tr("Ctrl+Shift+Tab")).
        autoRepeat(false);

    factory(Qn::SelectAllAction).
        flags(Qn::GlobalHotkey).
        text(tr("Select All")).
        shortcut(tr("Ctrl+A")).
        shortcutContext(Qt::WidgetWithChildrenShortcut).
        autoRepeat(false);

    factory(Qn::SelectionChangeAction).
        flags(Qn::NoTarget).
        text(tr("Selection Changed"));

    factory(Qn::PreferencesLicensesTabAction).
        flags(Qn::NoTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission);

    factory(Qn::PreferencesSmtpTabAction).
        flags(Qn::NoTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission);

    factory(Qn::PreferencesNotificationTabAction).
        flags(Qn::NoTarget).
        icon(qnSkin->icon("events/filter.png")).
        text(tr("Filter..."));

    factory(Qn::ConnectAction).
        flags(Qn::NoTarget);

    factory(Qn::ReconnectAction).
        flags(Qn::NoTarget).
        text(tr("Reconnect to Server"));

    factory(Qn::FreespaceAction).
        flags(Qn::GlobalHotkey).
        text(tr("Go to Freespace Mode")).
        shortcut(tr("F11")).
        autoRepeat(false);

    factory(Qn::WhatsThisAction).
        flags(Qn::NoTarget).
        text(tr("Help")).
        icon(qnSkin->icon("titlebar/whats_this.png"));

    factory(Qn::ClearCacheAction).
        flags(Qn::NoTarget).
        text(tr("Clear cache"));

    factory(Qn::CameraDiagnosticsAction).
        flags(Qn::ResourceTarget | Qn::SingleTarget).
        text(tr("Check Camera Issues...")).
        condition(new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::Any, this));

    factory(Qn::OpenBusinessLogAction).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission).
        icon(qnSkin->icon("events/log.png")).
        text(tr("Event Log..."));

    factory(Qn::OpenBusinessRulesAction).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission).
        text(tr("Alarm/Event Rules..."));

    factory(Qn::StartVideoWallControlAction).
        flags(Qn::Tree | Qn::VideoWallReviewScene | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Control Video Wall")). //TODO: #VW #TR
        condition(new QnStartVideoWallControlActionCondition(this));

    factory(Qn::PushMyScreenToVideowallAction).
        flags(Qn::Tree | Qn::VideoWallReviewScene | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Push my screen")).
        condition(new QnDesktopCameraActionCondition(this));

    factory(Qn::QueueAppRestartAction).
        flags(Qn::NoTarget).
        text(tr("Restart application"));

    factory(Qn::SelectTimeServerAction).
        flags(Qn::NoTarget).
        text(tr("Select time server"));

    factory(Qn::PtzActivatePresetAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Go To Saved Position")).
        requiredPermissions(Qn::WritePtzPermission).
        condition(new QnPtzActionCondition(Qn::PresetsPtzCapability, false, this));

    factory(Qn::PtzActivateTourAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Activate PTZ Tour")).
        requiredPermissions(Qn::WritePtzPermission).
        condition(new QnPtzActionCondition(Qn::ToursPtzCapability, false, this));

    factory(Qn::PtzActivateObjectAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Activate PTZ object")).
        requiredPermissions(Qn::WritePtzPermission);

    /* Context menu actions. */

    factory(Qn::FitInViewAction).
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Fit in View"));

    factory().
        flags(Qn::Scene).
        separator();

    factory(Qn::MainMenuAction).
        flags(Qn::GlobalHotkey).
        text(tr("Main Menu")).
#ifndef Q_OS_MACX
        shortcut(tr("Alt+Space")).
#endif
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/main_menu.png"));

    factory(Qn::OpenLoginDialogAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        text(tr("Connect to Server...")).
        shortcut(tr("Ctrl+Shift+C")).
        icon(qnSkin->icon("titlebar/disconnected.png")).
        autoRepeat(false);

    factory(Qn::DisconnectAction).
        flags(Qn::Main).
        text(tr("Logout")).
        autoRepeat(false).
        condition(new QnLoggedInCondition(this));

    factory().
        flags(Qn::Main).
        separator();

    factory(Qn::TogglePanicModeAction).
        flags(Qn::GlobalHotkey| Qn::DevMode).
        text(tr("Start Panic Recording")).
        toggledText(tr("Stop Panic Recording")).
        autoRepeat(false).
        shortcut(tr("Ctrl+P")).
//        icon(qnSkin->icon("titlebar/panic.png")).
        //requiredPermissions(Qn::CurrentMediaServerResourcesRole, Qn::ReadWriteSavePermission).
        condition(new QnPanicActionCondition(this));

    factory().
        flags(Qn::Main | Qn::Tree).
        separator();

    factory().
        flags(Qn::Main | Qn::TitleBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("New..."));

    factory.beginSubMenu(); {
        factory(Qn::NewUserLayoutAction).
            flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
            requiredPermissions(Qn::CreateLayoutPermission).
            text(tr("Layout...")).
            pulledText(tr("New Layout...")).
            condition(hasFlags(Qn::user));

        factory(Qn::OpenNewTabAction).
            flags(Qn::Main | Qn::TitleBar | Qn::SingleTarget | Qn::NoTarget | Qn::GlobalHotkey).
            text(tr("Tab")).
            pulledText(tr("New Tab")).
            shortcut(tr("Ctrl+T")).
            autoRepeat(false). /* Technically, it should be auto-repeatable, but we don't want the user opening 100500 layouts and crashing the client =). */
            icon(qnSkin->icon("titlebar/new_layout.png"));

        factory(Qn::OpenNewWindowAction).
            flags(Qn::Main | Qn::GlobalHotkey).
            text(tr("Window")).
            pulledText(tr("New Window")).
            shortcut(tr("Ctrl+N")).
            autoRepeat(false).
            condition(new QnLightModeCondition(Qn::LightModeNoNewWindow, this));

        factory(Qn::NewUserAction).
            flags(Qn::Main | Qn::Tree).
            requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditUsersPermission).
            text(tr("User...")).
            pulledText(tr("New User...")).
            condition(new QnTreeNodeTypeCondition(Qn::UsersNode, this)).
            autoRepeat(false);

        factory(Qn::NewVideoWallAction).
            flags(Qn::Main).
            requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
            text(tr("Video Wall...")).
            pulledText(tr("New Video Wall...")).
            autoRepeat(false);

    } factory.endSubMenu();

    factory(Qn::OpenCurrentUserLayoutMenu).
        flags(Qn::TitleBar | Qn::SingleTarget | Qn::NoTarget).
        text(tr("Open Layout...")).
        childFactory(new QnOpenCurrentUserLayoutActionFactory(this)).
        icon(qnSkin->icon("titlebar/dropdown.png"));

    factory().
        flags(Qn::TitleBar).
        separator();

    factory().
        flags(Qn::Main | Qn::Scene).
        text(tr("Open..."));

    factory.beginSubMenu(); {
        factory(Qn::OpenFileAction).
            flags(Qn::Main | Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey).
            requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
            text(tr("File(s)...")).
            shortcut(tr("Ctrl+O")).
            autoRepeat(false).
            icon(qnSkin->icon("folder.png"));

        factory(Qn::OpenLayoutAction).
            //flags(Qn::Main | Qn::Scene). // TODO
            forbiddenPermissions(Qn::CurrentLayoutResourceRole, Qn::AddRemoveItemsPermission).
            text(tr("Layout(s)...")).
            autoRepeat(false);

        factory(Qn::OpenFolderAction).
            flags(Qn::Main | Qn::Scene).
            requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
            text(tr("Folder..."));
    } factory.endSubMenu();

    factory(Qn::SaveCurrentLayoutAction).
        flags(Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey | Qn::IntentionallyAmbiguous).
        requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::SavePermission).
        text(tr("Save Current Layout")).
        shortcut(tr("Ctrl+S")).
        autoRepeat(false). /* There is no point in saving the same layout many times in a row. */
        condition(new QnSaveLayoutActionCondition(true, this));

    factory(Qn::SaveCurrentLayoutAsAction).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::CreateLayoutPermission).
        flags(Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey).
        text(tr("Save Current Layout As...")).
        shortcut(tr("Ctrl+Alt+S")).
        autoRepeat(false).
        condition(new QnSaveLayoutAsActionCondition(true, this));

    factory(Qn::SaveCurrentVideoWallReviewAction).
        flags(Qn::Main | Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey | Qn::IntentionallyAmbiguous).
        text(tr("Save Video Wall View")). //TODO: #VW #TR
        shortcut(tr("Ctrl+S")).
        autoRepeat(false).
        condition(new QnSaveVideowallReviewActionCondition(true, this));

    factory(Qn::DropOnVideoWallItemAction).
        flags(Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::VideoWallItemTarget | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Drop Resources"));

    factory().
        flags(Qn::Main).
        separator();

    if (QnScreenRecorder::isSupported()) {
        factory(Qn::ToggleScreenRecordingAction).
            flags(Qn::Main | Qn::GlobalHotkey).
            text(tr("Start Screen Recording")).
            toggledText(tr("Stop Screen Recording")).
            shortcut(tr("Alt+R")).
            shortcut(Qt::Key_MediaRecord).
            shortcutContext(Qt::ApplicationShortcut).
            autoRepeat(false).
            icon(qnSkin->icon("titlebar/recording.png", "titlebar/recording.png")).
            rotationSpeed(180.0);

        factory().
            flags(Qn::Main).
            separator();
    }

    factory(Qn::EscapeHotkeyAction).
        flags(Qn::GlobalHotkey).
        autoRepeat(false).
        shortcut(tr("Esc")).
        text(tr("Stop current action"));

    factory(Qn::FullscreenAction).
        flags(Qn::NoTarget).
        text(tr("Go to Fullscreen")).
        toggledText(tr("Exit Fullscreen")).
        icon(qnSkin->icon("titlebar/fullscreen.png", "titlebar/unfullscreen.png"));


    factory(Qn::MinimizeAction).
        flags(Qn::NoTarget).
        text(tr("Minimize")).
        icon(qnSkin->icon("titlebar/minimize.png"));

    factory(Qn::MaximizeAction).
        flags(Qn::NoTarget).
        text(tr("Maximize")).
        toggledText(tr("Restore Down")).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/fullscreen.png", "titlebar/unfullscreen.png"));


    factory(Qn::FullscreenMaximizeHotkeyAction).
        flags(Qn::GlobalHotkey).
        autoRepeat(false).
#ifdef Q_OS_MAC
        shortcut(tr("Ctrl+F")).
#else
        shortcut(tr("Alt+Enter")).
        shortcut(tr("Alt+Return")).
#endif
        shortcutContext(Qt::ApplicationShortcut);


    factory(Qn::MessageBoxAction).
        flags(Qn::NoTarget).
        text(tr("Show Message"));

    factory(Qn::VersionMismatchMessageAction).
        flags(Qn::NoTarget).
        text(tr("Show Version Mismatch Message"));

    factory(Qn::BetaVersionMessageAction).
        flags(Qn::NoTarget).
        text(tr("Show Beta Version Warning Message"));

    factory(Qn::BrowseUrlAction).
        flags(Qn::NoTarget).
        text(tr("Open in Browser..."));

    factory(Qn::SystemAdministrationAction).
        flags(Qn::Main | Qn::Tree).
        text(tr("System Administration...")).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission).
        condition(new QnTreeNodeTypeCondition(Qn::ServersNode, this));

    factory(Qn::SystemUpdateAction).
        flags(Qn::NoTarget).
        text(tr("System Update...")).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission);

    factory(Qn::PreferencesGeneralTabAction).
        flags(Qn::Main).
        text(tr("Local Settings...")).
        //shortcut(tr("Ctrl+P")).
        role(QAction::PreferencesRole).
        autoRepeat(false);

    factory(Qn::WebClientAction).
        flags(Qn::Main | Qn::Tree).
        text(tr("Open Web Client...")).
        autoRepeat(false).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalViewLivePermission).
        condition(new QnTreeNodeTypeCondition(Qn::ServersNode, this));

    factory().
        flags(Qn::Main).
        separator();

    factory(Qn::BusinessEventsAction).
        flags(Qn::GlobalHotkey).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission).
        text(tr("Alarm/Event Rules...")).
        icon(qnSkin->icon("events/settings.png")).
        shortcut(tr("Ctrl+E")).
        autoRepeat(false);

    factory(Qn::BusinessEventsLogAction).
        flags(Qn::GlobalHotkey).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission).
        text(tr("Event Log...")).
        icon(qnSkin->icon("events/log.png")).
        shortcut(tr("Ctrl+L")).
        autoRepeat(false);

    factory(Qn::CameraListAction).
        flags(Qn::GlobalHotkey).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission).
        text(tr("Camera List...")).
        shortcut(tr("Ctrl+M")).
        autoRepeat(false);

    factory(Qn::MergeSystems).
        flags(Qn::Main | Qn::Tree).
        text(tr("Merge Systems...")).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission).
        condition(new QnTreeNodeTypeCondition(Qn::ServersNode, this));

    factory().
        flags(Qn::Main).
        separator();

    factory(Qn::ShowcaseAction).
        flags(Qn::Main).
        text(tr("How-to Videos and FAQ...")).
        condition(new QnShowcaseActionCondition(this));

    factory(Qn::AboutAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        text(tr("About...")).
        shortcut(tr("F1")).
        shortcutContext(Qt::ApplicationShortcut).
        role(QAction::AboutRole).
        autoRepeat(false);

    factory().
        flags(Qn::Main).
        separator();

    factory(Qn::ExitAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        text(tr("Exit")).
        shortcut(tr("Alt+F4")).
        shortcutContext(Qt::ApplicationShortcut).
        role(QAction::QuitRole).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/exit.png"));

    factory(Qn::ExitActionDelayed).
        flags(Qn::NoTarget);

    factory(Qn::BeforeExitAction).
        flags(Qn::NoTarget);

    factory().
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        childFactory(new QnEdgeNodeActionFactory(this)).
        text(tr("Server...")).
        condition(new QnTreeNodeTypeCondition(Qn::EdgeNode, this));

    factory().
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        separator().
        condition(new QnTreeNodeTypeCondition(Qn::EdgeNode, this));

    /* Resource actions. */
    factory(Qn::OpenInLayoutAction).
        flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredPermissions(Qn::LayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
        text(tr("Open in Layout"));

    factory(Qn::OpenInCurrentLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
        text(tr("Open")).
        conditionalText(tr("Monitor"), hasFlags(Qn::server), Qn::All).
        condition(new QnOpenInCurrentLayoutActionCondition(this));

    factory(Qn::OpenInNewLayoutAction).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in New Tab")).
        conditionalText(tr("Monitor in a New Tab"), hasFlags(Qn::server), Qn::All).
        condition(new QnConjunctionActionCondition(
                      new QnOpenInNewEntityActionCondition(this),
                      new QnNegativeActionCondition(new QnResourceStatusActionCondition(Qn::Incompatible, true, this), this),
                      this));

    factory(Qn::OpenInNewWindowAction).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in New Window")).
        conditionalText(tr("Monitor in a New Window"), hasFlags(Qn::server), Qn::All).
        condition(new QnConjunctionActionCondition(
                      new QnOpenInNewEntityActionCondition(this),
                      new QnLightModeCondition(Qn::LightModeNoNewWindow, this),
                      new QnNegativeActionCondition(new QnResourceStatusActionCondition(Qn::Incompatible, true, this), this),
                      this));

    factory(Qn::OpenSingleLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Open Layout in a New Tab")).
        condition(hasFlags(Qn::layout));

    factory(Qn::OpenMultipleLayoutsAction).
        flags(Qn::Tree | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layouts")).
        condition(hasFlags(Qn::layout));

    factory(Qn::OpenLayoutsInNewWindowAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layout(s) in a New Window")). // TODO: #Elric split into sinle- & multi- action
        condition(new QnConjunctionActionCondition(
                      new QnResourceActionCondition(hasFlags(Qn::layout), Qn::All, this),
                      new QnLightModeCondition(Qn::LightModeNoNewWindow, this),
                      this));

    factory(Qn::OpenCurrentLayoutInNewWindowAction).
        flags(Qn::NoTarget).
        text(tr("Open Current Layout in a New Window")).
        condition(new QnLightModeCondition(Qn::LightModeNoNewWindow, this));

    factory(Qn::OpenAnyNumberOfLayoutsAction).
        flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layout(s)")).
        condition(hasFlags(Qn::layout));

    factory(Qn::OpenVideoWallsReviewAction).
       flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
       text(tr("Open Video Wall(s)")). //TODO: #VW #TR
       condition(hasFlags(Qn::videowall));

    factory(Qn::OpenInFolderAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Open Containing Folder")).
        shortcut(tr("Ctrl+Enter")).
        shortcut(tr("Ctrl+Return")).
        autoRepeat(false).
        condition(new QnOpenInFolderActionCondition(this));

    factory(Qn::IdentifyVideoWallAction).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::VideoWallItemTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Identify")).
        autoRepeat(false).
        condition(new QnIdentifyVideoWallActionCondition(this));

    factory(Qn::AttachToVideoWallAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Attach to Video Wall...")).
        autoRepeat(false).
        condition(hasFlags(Qn::videowall));

    factory(Qn::StartVideoWallAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Switch to Video Wall mode...")).  //TODO: #VW #TR
        autoRepeat(false).
        condition(new QnStartVideowallActionCondition(this));

    factory(Qn::SaveVideoWallReviewAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Save Video Wall View")). //TODO: #VW #TR
        shortcut(tr("Ctrl+S")).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        autoRepeat(false).
        condition(new QnSaveVideowallReviewActionCondition(false, this));

    factory(Qn::SaveVideowallMatrixAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Save Current Matrix")).
        autoRepeat(false).
        condition(new QnNonEmptyVideowallActionCondition(this));

    factory(Qn::LoadVideowallMatrixAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::VideoWallMatrixTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Load Matrix"));

    factory(Qn::DeleteVideowallMatrixAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallMatrixTarget | Qn::IntentionallyAmbiguous).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Delete")).
        shortcut(tr("Del")).
        autoRepeat(false);

    factory(Qn::ResetVideoWallLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget).
        text(tr("Update Layout")).
        autoRepeat(false).
        condition(new QnResetVideoWallLayoutActionCondition(this));

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(Qn::StopVideoWallAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Stop Video Wall")).
        autoRepeat(false).
        condition(new QnRunningVideowallActionCondition(this));

    factory(Qn::DetachFromVideoWallAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Detach Layout")).
        autoRepeat(false).
        condition(new QnDetachFromVideoWallActionCondition(this));

    factory(Qn::SaveLayoutAction).
        flags(Qn::TitleBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::SavePermission).
        text(tr("Save Layout")).
        condition(new QnSaveLayoutActionCondition(false, this));

    factory(Qn::SaveLayoutAsAction).
        flags(Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::UserResourceRole, Qn::CreateLayoutPermission).
        text(tr("Save Layout As...")).
        condition(new QnSaveLayoutAsActionCondition(false, this));

    factory(Qn::SaveLayoutForCurrentUserAsAction).
        flags(Qn::TitleBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::CreateLayoutPermission).
        text(tr("Save Layout As...")).
        condition(new QnSaveLayoutAsActionCondition(false, this));

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(Qn::DeleteVideoWallItemAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget | Qn::IntentionallyAmbiguous).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Delete")).
        shortcut(tr("Del")).
        autoRepeat(false);

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

    factory(Qn::ShowInfoAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Show Info")).
        shortcut(tr("Alt+I")).
        condition(new QnDisplayInfoActionCondition(false, this));

    factory(Qn::HideInfoAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Hide Info")).
        shortcut(tr("Alt+I")).
        condition(new QnDisplayInfoActionCondition(true, this));

    factory(Qn::ToggleInfoAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::HotkeyOnly).
        text(tr("Toggle Info")).
        shortcut(tr("Alt+I")).
        condition(new QnDisplayInfoActionCondition(this));

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Change Resolution...")).
        condition(new QnChangeResolutionActionCondition(this));

    factory.beginSubMenu(); {
        factory.beginGroup();
        factory(Qn::RadassAutoAction).
            flags(Qn::Scene | Qn::NoTarget ).
            text(tr("Auto")).
            checkable().
            checked();

        factory(Qn::RadassLowAction).
            flags(Qn::Scene | Qn::NoTarget ).
            text(tr("Low")).
            checkable();

        factory(Qn::RadassHighAction).
            flags(Qn::Scene | Qn::NoTarget ).
            text(tr("High")).
            checkable();
        factory.endGroup();
    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::SingleTarget).
        childFactory(new QnPtzPresetsToursActionFactory(this)).
        text(tr("PTZ...")).
        requiredPermissions(Qn::WritePtzPermission).
        condition(new QnPtzActionCondition(Qn::PresetsPtzCapability, false, this));

    factory.beginSubMenu(); {

        factory(Qn::PtzSavePresetAction).
            flags(Qn::Scene | Qn::SingleTarget).
            text(tr("Save Current Position...")).
            requiredPermissions(Qn::WritePtzPermission).
            condition(new QnPtzActionCondition(Qn::PresetsPtzCapability, true, this));

        factory(Qn::PtzManageAction).
            flags(Qn::Scene | Qn::SingleTarget).
            text(tr("Manage...")).
            requiredPermissions(Qn::WritePtzPermission).
            condition(new QnPtzActionCondition(Qn::ToursPtzCapability, false, this));

    } factory.endSubMenu();

    factory(Qn::PtzCalibrateFisheyeAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Calibrate Fisheye")).
        condition(new QnPtzActionCondition(Qn::VirtualPtzCapability, false, this));

#if 0
    factory(Qn::ToggleRadassAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::HotkeyOnly).
        text(tr("Toggle Resolution Mode")).
        shortcut(tr("Alt+I")).
        condition(new QnDisplayInfoActionCondition(this));
#endif

    factory(Qn::StartSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Show Motion/Smart Search")).
        conditionalText(tr("Show Motion"), new QnNoArchiveActionCondition(this)).
        shortcut(tr("Alt+G")).
        condition(new QnSmartSearchActionCondition(false, this));

    factory(Qn::StopSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Hide Motion/Smart Search")).
        conditionalText(tr("Hide Motion"), new QnNoArchiveActionCondition(this)).
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
        flags(Qn::Scene | Qn::SingleTarget | Qn::HotkeyOnly).
        text(tr("Take Screenshot")).
        shortcut(tr("Alt+S")).
        autoRepeat(false).
        condition(new QnTakeScreenshotActionCondition(this));

    factory(Qn::AdjustVideoAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Image Enhancement...")).
        shortcut(tr("Alt+J")).
        autoRepeat(false).
        condition(new QnAdjustVideoActionCondition(this));

    factory(Qn::CreateZoomWindowAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Create Zoom Window")).
        condition(new QnCreateZoomWindowActionCondition(this));

    factory().
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Rotate to..."));

    factory.beginSubMenu();{
        factory(Qn::Rotate0Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("0 degrees")).
            condition(new QnRotateItemCondition(this));

        factory(Qn::Rotate90Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("90 degrees")).
            condition(new QnRotateItemCondition(this));

        factory(Qn::Rotate180Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("180 degrees")).
            condition(new QnRotateItemCondition(this));

        factory(Qn::Rotate270Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("270 degrees")).
            condition(new QnRotateItemCondition(this));
    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(Qn::RemoveLayoutItemAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::LayoutItemTarget | Qn::IntentionallyAmbiguous).
        text(tr("Remove from Layout")).
#ifdef Q_OS_MACX
        shortcut(Qt::Key_Backspace).
#else
        shortcut(tr("Del")).
#endif
        autoRepeat(false).
        condition(new QnLayoutItemRemovalActionCondition(this));

    factory(Qn::RemoveFromServerAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::IntentionallyAmbiguous).
        requiredPermissions(Qn::RemovePermission).
        text(tr("Delete")).
#ifdef Q_OS_MACX
        shortcut(Qt::Key_Backspace).
#else
        shortcut(tr("Del")).
#endif
        autoRepeat(false).
        condition(new QnResourceRemovalActionCondition(this));


    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(Qn::RenameAction).
        flags(Qn::Tree | Qn::SingleTarget |  Qn::ResourceTarget | Qn::VideoWallItemTarget | Qn::VideoWallMatrixTarget).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalEditVideoWallPermission).
        text(tr("Rename")).
        shortcut(tr("F2")).
        autoRepeat(false).
        condition(new QnConjunctionActionCondition(
                      new QnRenameActionCondition(this),
                      new QnNegativeActionCondition(new QnResourceStatusActionCondition(Qn::Incompatible, true, this), this),
                      this));

    factory().
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        separator();

    factory(Qn::DeleteFromDiskAction).
        //flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget). // TODO
        text(tr("Delete from Disk")).
        autoRepeat(false).
        condition(hasFlags(Qn::url | Qn::local | Qn::media));

    factory(Qn::SetAsBackgroundAction).
        flags(Qn::Scene | Qn::SingleTarget).
        requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission).
        text(tr("Set as Layout Background")).
        autoRepeat(false).
        condition(new QnConjunctionActionCondition(
            new QnSetAsBackgroundActionCondition(this),
            new QnLightModeCondition(Qn::LightModeNoLayoutBackground, this),
            this));

    factory(Qn::UserSettingsAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("User Settings...")).
        condition(hasFlags(Qn::user));

    factory(Qn::CameraIssuesAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Check Camera Issues...")).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::Any, this),
            new QnPreviewSearchModeCondition(true, this),
            this));

    factory(Qn::CameraBusinessRulesAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Camera Rules...")).
        requiredPermissions(Qn::CurrentUserResourceRole, Qn::GlobalProtectedPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::ExactlyOne, this),
            new QnPreviewSearchModeCondition(true, this),
            this));

    factory(Qn::CameraSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Camera Settings...")).
        requiredPermissions(Qn::WritePermission).
        condition(new QnConjunctionActionCondition(
             new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::Any, this),
             new QnPreviewSearchModeCondition(true, this),
             this));

    factory(Qn::PictureSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Picture Settings...")).
        condition(new QnResourceActionCondition(hasFlags(Qn::still_image), Qn::Any, this));

    factory(Qn::LayoutSettingsAction).
       flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
       text(tr("Layout Settings...")).
       requiredPermissions(Qn::EditLayoutSettingsPermission).
       condition(new QnLightModeCondition(Qn::LightModeNoLayoutBackground, this));

    factory(Qn::VideowallSettingsAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Video Wall Settings...")).     //TODO: #VW #TR
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::videowall), Qn::ExactlyOne, this),
            new QnAutoStartAllowedActionCodition(this),
            this));

    factory(Qn::OpenInCameraSettingsDialogAction).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in Camera Settings Dialog"));

    factory(Qn::ServerAddCameraManuallyAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Add Camera(s)...")).
        condition(new QnConjunctionActionCondition(
                      new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
                      new QnEdgeServerCondition(false, this),
                      new QnNegativeActionCondition(new QnResourceStatusActionCondition(Qn::Incompatible, true, this), this),
                      this));

    factory(Qn::CameraListByServerAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Camera(s) List by Server...")).
        condition(new QnConjunctionActionCondition(
                      new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
                      new QnEdgeServerCondition(false, this),
                      new QnNegativeActionCondition(new QnResourceStatusActionCondition(Qn::Incompatible, true, this), this),
                      this));

    factory(Qn::PingAction).
        flags(Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Ping..."));

    factory(Qn::ServerLogsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Logs...")).
        condition(new QnConjunctionActionCondition(
                      new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
                      new QnNegativeActionCondition(new QnResourceStatusActionCondition(Qn::Incompatible, true, this), this),
                      this));

    factory(Qn::ServerIssuesAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Diagnostics...")).
        condition(new QnConjunctionActionCondition(
                      new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
                      new QnNegativeActionCondition(new QnResourceStatusActionCondition(Qn::Incompatible, true, this), this),
                      this));

    factory(Qn::ServerSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Settings...")).
        requiredPermissions(Qn::WritePermission).
        condition(new QnConjunctionActionCondition(
                      new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
                      new QnNegativeActionCondition(new QnResourceStatusActionCondition(Qn::Incompatible, true, this), this),
                      this));

    factory(Qn::ConnectToCurrentSystem).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Connect to the Current System...")).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::Any, this),
            new QnDisjunctionActionCondition(
                      new QnResourceStatusActionCondition(Qn::Incompatible, false, this),
                      new QnResourceStatusActionCondition(Qn::Unauthorized, false, this),
                      this),
            this));

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Change Cell Aspect Ratio...")).
        condition(new QnVideoWallReviewModeCondition(true, this));

    factory.beginSubMenu(); {
        factory.beginGroup();

        factory(Qn::SetCurrentLayoutAspectRatio4x3Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("4:3")).
            checkable().
            checked(qnGlobals->defaultLayoutCellAspectRatio() == 4.0/3.0);

        factory(Qn::SetCurrentLayoutAspectRatio16x9Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("16:9")).
            checkable().
            checked(qnGlobals->defaultLayoutCellAspectRatio() == 16.0/9.0);

        factory.endGroup();
    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Change Cell Spacing..."));

    factory.beginSubMenu(); {
        factory.beginGroup();

        factory(Qn::SetCurrentLayoutItemSpacing0Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("None")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing().width() == 0.0);

        factory(Qn::SetCurrentLayoutItemSpacing10Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Small")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing().width() == 0.1);

        factory(Qn::SetCurrentLayoutItemSpacing20Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Medium")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing().width() == 0.2);

        factory(Qn::SetCurrentLayoutItemSpacing30Action).
            flags(Qn::Scene | Qn::NoTarget).
            requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Large")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing().width() == 0.3);
        factory.endGroup();

    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        separator();

    factory(Qn::ToggleTourModeAction).
        flags(Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey).
        text(tr("Start Tour")).
        toggledText(tr("Stop Tour")).
        shortcut(tr("Alt+T")).
        autoRepeat(false).
        condition(new QnToggleTourActionCondition(this));

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        separator();

    factory(Qn::CurrentLayoutSettingsAction).
        flags(Qn::Scene | Qn::NoTarget).
        requiredPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission).
        text(tr("Layout Settings...")).
        condition(new QnLightModeCondition(Qn::LightModeNoLayoutBackground, this));

    /* Tab bar actions. */
    factory().
        flags(Qn::TitleBar).
        separator();

    factory(Qn::CloseLayoutAction).
        flags(Qn::TitleBar | Qn::ScopelessHotkey | Qn::SingleTarget).
        text(tr("Close")).
        shortcut(tr("Ctrl+W")).
        autoRepeat(false);

    factory(Qn::CloseAllButThisLayoutAction).
        flags(Qn::TitleBar | Qn::SingleTarget).
        text(tr("Close All But This")).
        condition(new QnLayoutCountActionCondition(2, this));

    /* Slider actions. */
    factory(Qn::StartTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Mark Selection Start")).
        shortcut(tr("[")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new QnTimePeriodActionCondition(Qn::NullTimePeriod, Qn::InvisibleAction, this));

    factory(Qn::EndTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Mark Selection End")).
        shortcut(tr("]")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new QnTimePeriodActionCondition(Qn::EmptyTimePeriod, Qn::InvisibleAction, this));

    factory(Qn::ClearTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Clear Selection")).
        condition(new QnTimePeriodActionCondition(Qn::EmptyTimePeriod | Qn::NormalTimePeriod, Qn::InvisibleAction, this));

    factory(Qn::ZoomToTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Zoom to Selection")).
        condition(new QnTimePeriodActionCondition(Qn::NormalTimePeriod, Qn::InvisibleAction, this));

    factory(Qn::AddCameraBookmarkAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Bookmark Selection...")).
        condition(new QnAddBookmarkActionCondition(this));

    factory(Qn::EditCameraBookmarkAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Edit Bookmark...")).
        condition(new QnModifyBookmarkActionCondition(this));

    factory(Qn::RemoveCameraBookmarkAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Remove Bookmark...")).
        condition(new QnModifyBookmarkActionCondition(this));

    factory().
        flags(Qn::Slider | Qn::SingleTarget).
        separator();

    factory(Qn::ExportTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Export Selected Area...")).
        requiredPermissions(Qn::ExportPermission).
        condition(new QnExportActionCondition(true, this));

    factory(Qn::ExportLayoutAction).
        flags(Qn::Slider | Qn::SingleTarget | Qn::MultiTarget | Qn::NoTarget).
        text(tr("Export Multi-Video...")).
        requiredPermissions(Qn::CurrentLayoutMediaItemsRole, Qn::ExportPermission).
        condition(new QnExportActionCondition(false, this));

    factory(Qn::ThumbnailsSearchAction).
        flags(Qn::Slider | Qn::Scene | Qn::SingleTarget).
        text(tr("Preview Search...")).
        condition(new QnPreviewActionCondition(this));



    factory(Qn::DebugIncrementCounterAction).
        flags(Qn::GlobalHotkey).
        shortcut(tr("Ctrl+Alt+Shift++")).
        text(tr("Increment Debug Counter"));

    factory(Qn::DebugDecrementCounterAction).
        flags(Qn::GlobalHotkey).
        shortcut(tr("Ctrl+Alt+Shift+-")).
        text(tr("Decrement Debug Counter"));

    factory(Qn::DebugShowResourcePoolAction).
        flags(Qn::GlobalHotkey | Qn::DevMode).
        shortcut(tr("Ctrl+Alt+Shift+R")).
        text(tr("Show Resource Pool"));

    factory(Qn::DebugCalibratePtzAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::DevMode).
        text(tr("Calibrate PTZ"));

    factory(Qn::DebugGetPtzPositionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::DevMode).
        text(tr("Get PTZ Position"));

    factory(Qn::DebugControlPanelAction).
        flags(Qn::GlobalHotkey | Qn::DevMode).
        shortcut(tr("Ctrl+Alt+Shift+D")).
        text(tr("Debug Control Panel"));

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


    factory().
        flags(Qn::Slider | Qn::TitleBar | Qn::Tree).
        separator();

    factory(Qn::ToggleThumbnailsAction).
        flags(Qn::NoTarget).
        text(tr("Show Thumbnails")).
        toggledText(tr("Hide Thumbnails"));

    factory(Qn::ToggleCalendarAction).
        flags(Qn::NoTarget).
        text(tr("Show Calendar")).
        toggledText(tr("Hide Calendar"));

    factory(Qn::ToggleTitleBarAction).
        flags(Qn::NoTarget).
        text(tr("Show Title Bar")).
        toggledText(tr("Hide Title Bar")).
        condition(new QnToggleTitleBarActionCondition(this));

    factory(Qn::PinTreeAction).
        flags(Qn::Tree | Qn::NoTarget).
        text(tr("Pin Tree")).
        toggledText(tr("Unpin Tree")).
        condition(new QnTreeNodeTypeCondition(Qn::RootNode, this));

    factory(Qn::ToggleTreeAction).
        flags(Qn::NoTarget).
        text(tr("Show Tree")).
        toggledText(tr("Hide Tree")).
        condition(new QnTreeNodeTypeCondition(Qn::RootNode, this));

    factory(Qn::ToggleSliderAction).
        flags(Qn::NoTarget).
        text(tr("Show Timeline")).
        toggledText(tr("Hide Timeline"));

    factory(Qn::PinNotificationsAction).
        flags(Qn::Notifications | Qn::NoTarget).
        text(tr("Pin Notifications")).
        toggledText(tr("Unpin Notifications"));

    factory(Qn::ToggleBackgroundAnimationAction).
        flags(Qn::GlobalHotkey).
        shortcut(tr("Ctrl+Alt+T")).
        text(tr("Disable Background Animation")).
        toggledText(tr("Enable Background Animation")).
        checked(true).
        autoRepeat(false);

#ifdef QN_ENABLE_BOOKMARKS
    factory(Qn::ToggleBookmarksSearchAction).
        flags(Qn::GlobalHotkey).
        text(tr("Show Search Panel")).
        toggledText(tr("Hide Search Panel")).
        shortcut(tr("Ctrl+F")).
        autoRepeat(false);
#endif
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

    QN_SCOPED_VALUE_ROLLBACK(&m_parametersByMenu[NULL], parameters);
    QN_SCOPED_VALUE_ROLLBACK(&m_shortcutAction, action);
    action->trigger();
}

bool QnActionManager::triggerIfPossible(Qn::ActionId id, const QnActionParameters &parameters) {
    QnAction *action = m_actionById.value(id);
    if(action == NULL) {
        qnWarning("Invalid action id '%1'.", static_cast<int>(id));
        return false;
    }

    if(action->checkCondition(action->scope(), parameters) != Qn::EnabledAction) {
        return false;
    }

    QN_SCOPED_VALUE_ROLLBACK(&m_parametersByMenu[NULL], parameters);
    QN_SCOPED_VALUE_ROLLBACK(&m_shortcutAction, action);
    action->trigger();
    return true;
}

QMenu* QnActionManager::integrateMenu(QMenu *menu, const QnActionParameters &parameters) {
    if (!menu)
        return NULL;

    Q_ASSERT(!m_parametersByMenu.contains(menu));
    m_parametersByMenu[menu] = parameters;
    menu->installEventFilter(this);

    connect(menu, &QMenu::aboutToShow,  this, [this, menu] { emit menuAboutToShow(menu); });
    connect(menu, &QMenu::aboutToHide,  this, [this, menu] { emit menuAboutToHide(menu); });
    connect(menu, &QObject::destroyed, this, &QnActionManager::at_menu_destroyed);

    return menu;
}


QMenu *QnActionManager::newMenu(Qn::ActionScope scope, QWidget *parent, const QnActionParameters &parameters, CreationOptions options) {
    return newMenu(Qn::NoAction, scope, parent, parameters, options);
}

QMenu *QnActionManager::newMenu(Qn::ActionId rootId, Qn::ActionScope scope, QWidget *parent, const QnActionParameters &parameters, CreationOptions options) {
    QnAction *rootAction = rootId == Qn::NoAction ? m_root : action(rootId);

    QMenu *result = NULL;
    if(!rootAction) {
        qnWarning("No action exists for id '%1'.", static_cast<int>(rootId));
    } else {
        result = newMenuRecursive(rootAction, scope, parameters, parent, options);
        if (!result)
            result = integrateMenu(new QnMenu(parent), parameters);
    }

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
    dst->setSeparator(src->isSeparator());

    dst->setProperty(sourceActionPropertyName, QVariant::fromValue<QnAction *>(src));
    foreach(const QByteArray &name, src->dynamicPropertyNames())
        dst->setProperty(name.data(), src->property(name.data()));

    if(forwardSignals) {
        connect(dst, &QAction::triggered,   src, &QAction::trigger);
        connect(dst, &QAction::toggled,     src, &QAction::setChecked);
    }
}

QMenu *QnActionManager::newMenuRecursive(const QnAction *parent, Qn::ActionScope scope, const QnActionParameters &parameters, QWidget *parentWidget, CreationOptions options) {
    if (parent->childFactory()) {
        QMenu* childMenu = parent->childFactory()->newMenu(parameters, parentWidget);
        if (childMenu && childMenu->isEmpty()) {
            delete childMenu;
            return NULL;
        }

        /* Do not need to call integrateMenu, it is already integrated. */
        if (childMenu)
            return childMenu;
        /* Otherwise we should continue to main factory actions. */
    }

    QMenu *result = new QnMenu(parentWidget);

    if(!parent->children().isEmpty()) {
        foreach(QnAction *action, parent->children()) {
            Qn::ActionVisibility visibility;
            if(action->flags() & Qn::HotkeyOnly) {
                visibility = Qn::InvisibleAction;
            } else {
                visibility = action->checkCondition(scope, parameters);
            }
            if(visibility == Qn::InvisibleAction)
                continue;

            QMenu *menu = newMenuRecursive(action, scope, parameters, parentWidget, options);
            if((!menu || menu->isEmpty()) && (action->flags() & Qn::RequiresChildren))
                continue;

            QString replacedText;
            if(menu && menu->actions().size() == 1) {
                QnAction *menuAction = qnAction(menu->actions()[0]);
                if(menuAction && (menuAction->flags() & Qn::Pullable)) {
                    delete menu;
                    menu = NULL;

                    action = menuAction;
                    visibility = action->checkCondition(scope, parameters);
                    replacedText = action->pulledText();
                }
            }

            if(menu)
                connect(result, &QObject::destroyed, menu, &QObject::deleteLater);

            if (action->hasConditionalTexts())
                replacedText = action->checkConditionalText(parameters);

            QAction *newAction = NULL;
            if(!replacedText.isEmpty() || visibility == Qn::DisabledAction || menu != NULL || (options & DontReuseActions)) {
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
    }

    if(parent->childFactory()) {
        QList<QAction *> actions = parent->childFactory()->newActions(parameters, NULL);

        if(!actions.isEmpty()) {
            if (!result->isEmpty())
                result->addSeparator();

            foreach(QAction *action, actions) {
                action->setParent(result);
                result->addAction(action);
            }
        }
    }

    if (result->isEmpty()) {
        delete result;
        return NULL;
    }

    return integrateMenu(result, parameters);
}

QnActionParameters QnActionManager::currentParameters(QnAction *action) const {
    if(m_shortcutAction == action)
        return m_parametersByMenu.value(NULL);
    
    if(!m_parametersByMenu.contains(m_lastClickedMenu)) {
        qnWarning("No active menu, no target exists.");
        return QnActionParameters();
    }
    
    return m_parametersByMenu.value(m_lastClickedMenu);
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

        if(action->menu()) {
            if(redirectActionRecursive(action->menu(), sourceId, targetAction)) {
                if(action->menu()->isEmpty())
                    menu->removeAction(action);

                return true;
            }
        }
    }

    return false;
}

void QnActionManager::at_menu_destroyed(QObject *menu) {
    m_parametersByMenu.remove(menu);
    if (m_lastClickedMenu == menu)
        m_lastClickedMenu = NULL;
}

bool QnActionManager::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() != QEvent::MouseButtonRelease)
        return false;

    if (!dynamic_cast<QMenu*>(watched))
        return false;

    m_lastClickedMenu = watched;
    return false;
}
