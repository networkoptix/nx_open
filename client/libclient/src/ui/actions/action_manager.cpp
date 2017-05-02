#include "action_manager.h"

#include <cassert>

#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QGraphicsItem>

#include "action.h"
#include "action_factories.h"
#include "action_text_factories.h"
#include "action_conditions.h"
#include "action_target_provider.h"
#include "action_parameter_types.h"

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_criterion.h>
#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>

#include <ui/style/skin.h>
#include <ui/style/noptix_style.h>
#include <ui/style/globals.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/app_info.h>

namespace {
const char *sourceActionPropertyName = "_qn_sourceAction";

QnAction *qnAction(QAction *action)
{
    QnAction *result = action->property(sourceActionPropertyName).value<QnAction *>();
    if (result)
        return result;

    return dynamic_cast<QnAction *>(action);
}

class QnMenu: public QMenu
{
    typedef QMenu base_type;
public:
    explicit QnMenu(QWidget *parent = 0): base_type(parent) {}

protected:
    virtual void mousePressEvent(QMouseEvent *event) override
    {
        /* This prevents the click from propagating to the underlying widget. */
        setAttribute(Qt::WA_NoMouseReplay);
        base_type::mousePressEvent(event);
    }
};

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnActionBuilder
// -------------------------------------------------------------------------- //
class QnActionBuilder
{
public:

    enum ActionPlatform
    {
        AllPlatforms = -1,
        Windows,
        Linux,
        Mac
    };

    QnActionBuilder(QnAction *action):
        m_action(action)
    {
        action->setShortcutContext(Qt::WindowShortcut);
    }

    QnActionBuilder shortcut(const QKeySequence &keySequence, ActionPlatform platform, bool replaceExisting)
    {
        QGuiApplication* guiApp = qobject_cast<QGuiApplication*>(qApp);

        if (!guiApp)
            return *this;

        if (keySequence.isEmpty())
            return *this;

        bool set = false;

        switch (platform)
        {
            case Windows:
#ifdef Q_OS_WIN
                set = true;
#endif
                break;
            case Linux:
#ifdef Q_OS_LINUX
                set = true;
#endif
                break;
            case Mac:
#ifdef Q_OS_MAC
                set = true;
#endif
                break;
            default:
                set = true;
                break;
        }

        if (set)
        {
            QList<QKeySequence> shortcuts = m_action->shortcuts();
            if (replaceExisting)
                shortcuts.clear();
            shortcuts.append(keySequence);
            m_action->setShortcuts(shortcuts);
        }

        return *this;
    }
    QnActionBuilder shortcut(const QKeySequence &keySequence)
    {
        return shortcut(keySequence, AllPlatforms, false);
    }

    QnActionBuilder shortcutContext(Qt::ShortcutContext context)
    {
        m_action->setShortcutContext(context);

        return *this;
    }

    QnActionBuilder icon(const QIcon &icon)
    {
        m_action->setIcon(icon);

        return *this;
    }

    QnActionBuilder iconVisibleInMenu(bool visible)
    {
        m_action->setIconVisibleInMenu(visible);

        return *this;
    }

    QnActionBuilder role(QAction::MenuRole role)
    {
        m_action->setMenuRole(role);

        return *this;
    }

    QnActionBuilder autoRepeat(bool autoRepeat)
    {
        m_action->setAutoRepeat(autoRepeat);

        return *this;
    }

    QnActionBuilder text(const QString &text)
    {
        m_action->setText(text);
        m_action->setNormalText(text);

        return *this;
    }

    QnActionBuilder dynamicText(QnActionTextFactory* factory)
    {
        m_action->setTextFactory(factory);
        return *this;
    }

    QnActionBuilder toggledText(const QString &text)
    {
        m_action->setToggledText(text);
        m_action->setCheckable(true);

        showCheckBoxInMenu(false);

        return *this;
    }

    QnActionBuilder pulledText(const QString &text)
    {
        m_action->setPulledText(text);
        m_action->setFlags(m_action->flags() | Qn::Pullable);

        return *this;
    }

    QnActionBuilder toolTip(const QString &toolTip)
    {
        m_action->setToolTip(toolTip);

        return *this;
    }

    QnActionBuilder toolTipFormat(const QString &toolTipFormat)
    {
        m_action->setToolTipFormat(toolTipFormat);

        return *this;
    }

    QnActionBuilder flags(Qn::ActionFlags flags)
    {
        m_action->setFlags(m_action->flags() | flags);

        return *this;
    }

    QnActionBuilder mode(QnActionTypes::ClientModes mode)
    {
        m_action->setMode(mode);
        return *this;
    }

    QnActionBuilder requiredGlobalPermission(Qn::GlobalPermission permission)
    {
        m_action->setRequiredGlobalPermission(permission);

        return *this;
    }

    QnActionBuilder requiredTargetPermissions(int key, Qn::Permissions permissions)
    {
        m_action->setRequiredTargetPermissions(key, permissions);
        return *this;
    }

    QnActionBuilder requiredTargetPermissions(Qn::Permissions permissions)
    {
        m_action->setRequiredTargetPermissions(permissions);
        return *this;
    }

    QnActionBuilder separator(bool isSeparator = true)
    {
        m_action->setSeparator(isSeparator);
        m_action->setFlags(m_action->flags() | Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::WidgetTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::VideoWallItemTarget);

        return *this;
    }

    QnActionBuilder conditionalText(const QString &text, QnActionCondition *condition)
    {
        m_action->addConditionalText(condition, text);
        return *this;
    }

    QnActionBuilder conditionalText(const QString &text, const QnResourceCriterion &criterion, Qn::MatchMode matchMode = Qn::All)
    {
        m_action->addConditionalText(new QnResourceActionCondition(criterion, matchMode, m_action), text);
        return *this;
    }

    QnActionBuilder checkable(bool isCheckable = true)
    {
        m_action->setCheckable(isCheckable);

        return *this;
    }

    QnActionBuilder checked(bool isChecked = true)
    {
        m_action->setChecked(isChecked);

        return *this;
    }

    QnActionBuilder showCheckBoxInMenu(bool show)
    {
        m_action->setProperty(Qn::HideCheckBoxInMenu, !show);

        return *this;
    }

    QnActionBuilder condition(QnActionCondition *condition)
    {
        NX_ASSERT(m_action->condition() == NULL);

        m_action->setCondition(condition);

        return *this;
    }

    QnActionBuilder condition(const QnResourceCriterion &criterion, Qn::MatchMode matchMode = Qn::All)
    {
        NX_ASSERT(m_action->condition() == NULL);

        m_action->setCondition(new QnResourceActionCondition(criterion, matchMode, m_action));

        return *this;
    }

    QnActionBuilder childFactory(QnActionFactory *childFactory)
    {
        m_action->setChildFactory(childFactory);
        m_action->setFlags(m_action->flags() | Qn::RequiresChildren);

        return *this;
    }

private:
    QnAction *m_action;
};


// -------------------------------------------------------------------------- //
// QnMenuFactory
// -------------------------------------------------------------------------- //
class QnMenuFactory
{
public:
    QnMenuFactory(QnActionManager *menu, QnAction *parent):
        m_manager(menu),
        m_lastFreeActionId(QnActions::ActionCount),
        m_currentGroup(0)
    {
        m_actionStack.push_back(parent);
        m_lastAction = parent;
    }

    void beginSubMenu()
    {
        m_actionStack.push_back(m_lastAction);
    }

    void endSubMenu()
    {
        m_actionStack.pop_back();
    }

    void beginGroup()
    {
        m_currentGroup = new QActionGroup(m_manager);
    }

    void endGroup()
    {
        m_currentGroup = NULL;
    }

    QnActionBuilder operator()(QnActions::IDType id)
    {
        QnAction *action = m_manager->action(id);
        if (action == NULL)
        {
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

    QnActionBuilder operator()()
    {
        return operator()(static_cast<QnActions::IDType>(m_lastFreeActionId++));
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
QnAction *checkSender(QObject *sender)
{
    QnAction *result = qobject_cast<QnAction *>(sender);
    if (result == NULL)
        qnWarning("Cause cannot be determined for non-QnAction senders.");
    return result;
}

bool checkType(const QVariant &items)
{
    Qn::ActionParameterType type = QnActionParameterTypes::type(items);
    if (type == 0)
    {
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
    m_root = new QnAction(QnActions::NoAction, this);
    m_actionById[QnActions::NoAction] = m_root;
    m_idByAction[m_root] = QnActions::NoAction;

    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this, &QnActionManager::hideAllMenus);

    QnMenuFactory factory(this, m_root);

    using namespace QnResourceCriterionExpressions;



    /* Actions that are not assigned to any menu. */

    factory(QnActions::ShowFpsAction).
        flags(Qn::GlobalHotkey).
        text(tr("Show FPS")).
        toggledText(tr("Hide FPS")).
        shortcut(lit("Ctrl+Alt+F")).
        autoRepeat(false);

    factory(QnActions::ShowDebugOverlayAction).
        flags(Qn::GlobalHotkey).
        text(lit("Show Debug")).
        toggledText(lit("Hide Debug")).
        shortcut(lit("Ctrl+Alt+D")).
        autoRepeat(false);

    factory(QnActions::DropResourcesAction).
        flags(Qn::ResourceTarget | Qn::WidgetTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::SingleTarget | Qn::MultiTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Drop Resources"));

    factory(QnActions::DropResourcesIntoNewLayoutAction).
        flags(Qn::ResourceTarget | Qn::WidgetTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::SingleTarget | Qn::MultiTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Drop Resources into New Layout"));

    factory(QnActions::DelayedOpenVideoWallItemAction).
        flags(Qn::NoTarget).
        text(tr("Delayed Open Video Wall"));

    factory(QnActions::DelayedDropResourcesAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Delayed Drop Resources"));

    factory(QnActions::InstantDropResourcesAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Instant Drop Resources"));

    factory(QnActions::MoveCameraAction).
        flags(Qn::ResourceTarget | Qn::SingleTarget | Qn::MultiTarget).
        requiredTargetPermissions(Qn::RemovePermission).
        text(QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Move Devices"),
            tr("Move Cameras")
        )).
        condition(hasFlags(Qn::network));

    factory(QnActions::NextLayoutAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Next Layout")).
        shortcut(lit("Ctrl+Tab")).
        autoRepeat(false);

    factory(QnActions::PreviousLayoutAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Previous Layout")).
        shortcut(lit("Ctrl+Shift+Tab")).
        autoRepeat(false);

    factory(QnActions::SelectAllAction).
        flags(Qn::GlobalHotkey).
        text(tr("Select All")).
        shortcut(lit("Ctrl+A")).
        shortcutContext(Qt::WidgetWithChildrenShortcut).
        autoRepeat(false);

    factory(QnActions::SelectionChangeAction).
        flags(Qn::NoTarget).
        text(tr("Selection Changed"));

    factory(QnActions::PreferencesLicensesTabAction).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::PreferencesSmtpTabAction).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::PreferencesNotificationTabAction).
        flags(Qn::NoTarget).
        icon(qnSkin->icon("events/filter.png")).
        text(tr("Filter..."));

    factory(QnActions::PreferencesCloudTabAction).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::ConnectAction).
        flags(Qn::NoTarget);

    factory(QnActions::ConnectToCloudSystemAction).
        flags(Qn::Tree | Qn::NoTarget).
        text(tr("Connect to System")).
        condition(new QnTreeNodeTypeCondition(Qn::CloudSystemNode, this));

    factory(QnActions::ReconnectAction).
        flags(Qn::NoTarget);

    factory(QnActions::FreespaceAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Go to Freespace Mode")).
        shortcut(lit("F11")).
        autoRepeat(false);

    factory(QnActions::WhatsThisAction).
        flags(Qn::NoTarget).
        text(tr("Help")).
        icon(qnSkin->icon("titlebar/window_question.png"));

    factory(QnActions::CameraDiagnosticsAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::ResourceTarget | Qn::SingleTarget).
        dynamicText(new QnDevicesNameActionTextFactory(
            QnCameraDeviceStringSet(
                tr("Device Diagnostics..."),
                tr("Camera Diagnostics..."),
                tr("I/O Module Diagnostics...")
            ), this)).
        condition(new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::Any, this));

    factory(QnActions::OpenBusinessLogAction).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget
            | Qn::LayoutItemTarget | Qn::WidgetTarget | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        requiredGlobalPermission(Qn::GlobalViewLogsPermission).
        icon(qnSkin->icon("events/log.png")).
        shortcut(lit("Ctrl+L")).
        text(tr("Event Log..."));

    factory(QnActions::OpenBusinessRulesAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Event Rules..."));

    factory(QnActions::OpenFailoverPriorityAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Failover Priority..."));

    factory(QnActions::OpenBackupCamerasAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Cameras to Backup..."));

    factory(QnActions::StartVideoWallControlAction).
        flags(Qn::Tree | Qn::VideoWallReviewScene | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Control Video Wall")).
        condition(new QnConjunctionActionCondition(
            new QnStartVideoWallControlActionCondition(this),
            new QnForbiddenInSafeModeCondition(this),
            this));

    factory(QnActions::PushMyScreenToVideowallAction).
        flags(Qn::Tree | Qn::VideoWallReviewScene | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Push my screen")).
        condition(new QnConjunctionActionCondition(
            new QnDesktopCameraActionCondition(this),
            new QnForbiddenInSafeModeCondition(this),
            this));

    factory(QnActions::QueueAppRestartAction).
        flags(Qn::NoTarget).
        text(tr("Restart application"));

    factory(QnActions::SelectTimeServerAction).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Select Time Server"));

    factory(QnActions::PtzActivatePresetAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Go To Saved Position")).
        requiredTargetPermissions(Qn::WritePtzPermission).
        condition(new QnPtzActionCondition(Qn::PresetsPtzCapability, false, this));

    factory(QnActions::PtzActivateTourAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Activate PTZ Tour")).
        requiredTargetPermissions(Qn::WritePtzPermission).
        condition(new QnPtzActionCondition(Qn::ToursPtzCapability, false, this));

    factory(QnActions::PtzActivateObjectAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Activate PTZ Object")).
        requiredTargetPermissions(Qn::WritePtzPermission);

    /* Context menu actions. */

    factory(QnActions::FitInViewAction).
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Fit in View"));

    factory().
        flags(Qn::Scene).
        separator();

    factory(QnActions::MainMenuAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Main Menu")).
        shortcut(lit("Alt+Space"), QnActionBuilder::Mac, true).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/main_menu.png"));

    factory(QnActions::OpenLoginDialogAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Connect to Server...")).
        shortcut(lit("Ctrl+Shift+C")).
        autoRepeat(false);

    factory(QnActions::DisconnectAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Disconnect from Server")).
        autoRepeat(false).
        shortcut(lit("Ctrl+Shift+D")).
        condition(new QnLoggedInCondition(this));

    factory(QnActions::ResourcesModeAction).
        flags(Qn::Main).
        mode(QnActionTypes::DesktopMode).
        text(tr("Browse Local Files")).
        toggledText(tr("Show Welcome Screen")).
        condition(new QnBrowseLocalFilesCondition(this));

    factory().
        flags(Qn::Main).
        separator();

    factory(QnActions::TogglePanicModeAction).
        flags(Qn::GlobalHotkey | Qn::DevMode).
        mode(QnActionTypes::DesktopMode).
        text(tr("Start Panic Recording")).
        toggledText(tr("Stop Panic Recording")).
        autoRepeat(false).
        shortcut(lit("Ctrl+P")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnPanicActionCondition(this));

    factory().
        flags(Qn::Main | Qn::Tree).
        separator();

    factory().
        flags(Qn::Main | Qn::TitleBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("New..."));

    factory.beginSubMenu();
    {
        factory(QnActions::OpenNewTabAction).
            flags(Qn::Main | Qn::TitleBar | Qn::SingleTarget | Qn::NoTarget | Qn::GlobalHotkey).
            mode(QnActionTypes::DesktopMode).
            text(tr("Tab")).
            pulledText(tr("New Tab")).
            shortcut(lit("Ctrl+T")).
            autoRepeat(false). /* Technically, it should be auto-repeatable, but we don't want the user opening 100500 layouts and crashing the client =). */
            icon(qnSkin->icon("titlebar/new_layout.png"));

        factory(QnActions::OpenNewWindowAction).
            flags(Qn::Main | Qn::GlobalHotkey).
            mode(QnActionTypes::DesktopMode).
            text(tr("Window")).
            pulledText(tr("New Window")).
            shortcut(lit("Ctrl+N")).
            autoRepeat(false).
            condition(new QnLightModeCondition(Qn::LightModeNoNewWindow, this));

        factory().
            flags(Qn::Main).
            separator();

        factory(QnActions::NewUserAction).
            flags(Qn::Main | Qn::Tree).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("User...")).
            pulledText(tr("New User...")).
            condition(new QnConjunctionActionCondition(
                new QnTreeNodeTypeCondition(Qn::UsersNode, this),
                new QnForbiddenInSafeModeCondition(this),
                this)
            ).
            autoRepeat(false);

        factory(QnActions::NewVideoWallAction).
            flags(Qn::Main).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("Video Wall...")).
            pulledText(tr("New Video Wall...")).
            condition(new QnForbiddenInSafeModeCondition(this)).
            autoRepeat(false);

        factory(QnActions::NewWebPageAction).
            flags(Qn::Main | Qn::Tree).
            requiredGlobalPermission(Qn::GlobalAdminPermission).
            text(tr("Web Page...")).
            pulledText(tr("New Web Page...")).
            condition(new QnConjunctionActionCondition(
                new QnTreeNodeTypeCondition(Qn::WebPagesNode, this),
                new QnForbiddenInSafeModeCondition(this),
                this)
            ).
            autoRepeat(false);

    }
    factory.endSubMenu();

    factory(QnActions::NewUserLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::NoTarget).
        text(tr("New Layout...")).
        condition(new QnConjunctionActionCondition(
            new QnNewUserLayoutActionCondition(this),
            new QnForbiddenInSafeModeCondition(this),
            this)
        );

    factory(QnActions::OpenCurrentUserLayoutMenu).
        flags(Qn::TitleBar | Qn::SingleTarget | Qn::NoTarget).
        text(tr("Open Layout...")).
        childFactory(new QnOpenCurrentUserLayoutActionFactory(this)).
        icon(qnSkin->icon("titlebar/dropdown.png"));

    factory().
        flags(Qn::TitleBar).
        separator();

    factory().
        flags(Qn::Main | Qn::Tree | Qn::Scene).
        mode(QnActionTypes::DesktopMode).
        text(tr("Open..."));

    factory.beginSubMenu();
    {
        factory(QnActions::OpenFileAction).
            flags(Qn::Main | Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey).
            mode(QnActionTypes::DesktopMode).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
            text(tr("File(s)...")).
            shortcut(lit("Ctrl+O")).
            autoRepeat(false);

        factory(QnActions::OpenFolderAction).
            flags(Qn::Main | Qn::Scene).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
            text(tr("Folder..."));

        factory().separator().
            flags(Qn::Main);

        factory(QnActions::WebClientAction).
            flags(Qn::Main | Qn::Tree | Qn::NoTarget).
            text(tr("Web Client...")).
            pulledText(tr("Open Web Client...")).
            autoRepeat(false).
            condition(new QnConjunctionActionCondition(
                new QnLoggedInCondition(this),
                new QnTreeNodeTypeCondition({Qn::CurrentSystemNode, Qn::ServersNode}, this),
                this));

    } factory.endSubMenu();

    factory(QnActions::SaveCurrentLayoutAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey | Qn::IntentionallyAmbiguous).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::SavePermission).
        text(tr("Save Current Layout")).
        shortcut(lit("Ctrl+S")).
        autoRepeat(false). /* There is no point in saving the same layout many times in a row. */
        condition(new QnSaveLayoutActionCondition(true, this));

    factory(QnActions::SaveCurrentLayoutAsAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey).
        text(tr("Save Current Layout As...")).
        shortcut(lit("Ctrl+Shift+S")).
        shortcut(lit("Ctrl+Alt+S"), QnActionBuilder::Windows, true).
        autoRepeat(false).
        condition(new QnConjunctionActionCondition(
            new QnLoggedInCondition(this),
            new QnSaveLayoutAsActionCondition(true, this),
            this));

    factory(QnActions::ShareLayoutAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::SingleTarget | Qn::ResourceTarget).
        text(lit("Share_Layout_with")).
        autoRepeat(false).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnForbiddenInSafeModeCondition(this));

    factory(QnActions::SaveCurrentVideoWallReviewAction).
        flags(Qn::Main | Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey | Qn::IntentionallyAmbiguous).
        mode(QnActionTypes::DesktopMode).
        text(tr("Save Video Wall View")).
        shortcut(lit("Ctrl+S")).
        autoRepeat(false).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        condition(new QnConjunctionActionCondition(
            new QnForbiddenInSafeModeCondition(this),
            new QnSaveVideowallReviewActionCondition(true, this),
            this));

    factory(QnActions::DropOnVideoWallItemAction).
        flags(Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::LayoutTarget | Qn::VideoWallItemTarget | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Drop Resources")).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        condition(new QnForbiddenInSafeModeCondition(this));

    factory().
        flags(Qn::Main).
        separator();

    const bool screenRecordingSupported =
#if defined(Q_OS_WIN)
        true;
#else
        false;
#endif

    if (screenRecordingSupported && qnRuntime->isDesktopMode())
    {
        factory(QnActions::ToggleScreenRecordingAction).
            flags(Qn::Main | Qn::GlobalHotkey).
            mode(QnActionTypes::DesktopMode).
            text(tr("Start Screen Recording")).
            toggledText(tr("Stop Screen Recording")).
            shortcut(lit("Alt+R")).
            shortcut(Qt::Key_MediaRecord).
            shortcutContext(Qt::ApplicationShortcut).
            autoRepeat(false);

        factory().
            flags(Qn::Main).
            separator();
    }

    factory(QnActions::EscapeHotkeyAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        autoRepeat(false).
        shortcut(lit("Esc")).
        text(tr("Stop current action"));

    factory(QnActions::FullscreenAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Go to Fullscreen")).
        toggledText(tr("Exit Fullscreen")).
        icon(qnSkin->icon("titlebar/window_maximize.png", "titlebar/window_restore.png"));


    factory(QnActions::MinimizeAction).
        flags(Qn::NoTarget).
        text(tr("Minimize")).
        icon(qnSkin->icon("titlebar/window_minimize.png"));

    factory(QnActions::MaximizeAction).
        flags(Qn::NoTarget).
        text(tr("Maximize")).
        toggledText(tr("Restore Down")).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/window_maximize.png", "titlebar/window_restore.png"));


    factory(QnActions::FullscreenMaximizeHotkeyAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        autoRepeat(false).
        shortcut(lit("Alt+Enter")).
        shortcut(lit("Alt+Return")).
        shortcut(lit("Ctrl+F"), QnActionBuilder::Mac, true).
        shortcutContext(Qt::ApplicationShortcut);

    factory(QnActions::VersionMismatchMessageAction).
        flags(Qn::NoTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(lit("Show Version Mismatch Message"));

    factory(QnActions::BetaVersionMessageAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(lit("Show Beta Version Warning Message"));

    factory(QnActions::AllowStatisticsReportMessageAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(lit("Ask About Statistics Reporting"));

    factory(QnActions::BrowseUrlAction).
        flags(Qn::NoTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Open in Browser..."));

    factory(QnActions::SystemAdministrationAction).
        flags(Qn::Main | Qn::Tree | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("System Administration...")).
        shortcut(lit("Ctrl+Alt+A")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnTreeNodeTypeCondition({Qn::CurrentSystemNode, Qn::ServersNode}, this));

    factory(QnActions::SystemUpdateAction).
        flags(Qn::NoTarget).
        text(tr("System Update...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission);

    factory(QnActions::UserManagementAction).
        flags(Qn::Main | Qn::Tree).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("User Management...")).
        condition(new QnTreeNodeTypeCondition(Qn::UsersNode, this));

    factory(QnActions::PreferencesGeneralTabAction).
        flags(Qn::Main).
        text(tr("Local Settings...")).
        //shortcut(lit("Ctrl+P")).
        role(QAction::PreferencesRole).
        autoRepeat(false);

    factory(QnActions::OpenAuditLogAction).
        flags(Qn::Main).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Audit Trail..."));

    factory(QnActions::OpenBookmarksSearchAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        requiredGlobalPermission(Qn::GlobalViewBookmarksPermission).
        text(tr("Bookmark Search...")).
        shortcut(lit("Ctrl+B")).
        autoRepeat(false);

    factory(QnActions::LoginToCloud).
        flags(Qn::NoTarget).
        text(tr("Log in to %1...", "Log in to Nx Cloud").arg(QnAppInfo::cloudName()));

    factory(QnActions::LogoutFromCloud).
        flags(Qn::NoTarget).
        text(tr("Log out from %1", "Log out from Nx Cloud").arg(QnAppInfo::cloudName()));

    factory(QnActions::OpenCloudMainUrl).
        flags(Qn::NoTarget).
        text(tr("Open %1 Portal...", "Open Nx Cloud Portal").arg(QnAppInfo::cloudName()));

    factory(QnActions::OpenCloudManagementUrl).
        flags(Qn::NoTarget).
        text(tr("Account Settings..."));

    factory(QnActions::HideCloudPromoAction).
        flags(Qn::NoTarget);

    factory(QnActions::OpenCloudRegisterUrl).
        flags(Qn::NoTarget).
        text(tr("Create Account..."));

    factory().
        flags(Qn::Main).
        separator();

    factory(QnActions::BusinessEventsAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Event Rules...")).
        icon(qnSkin->icon("events/settings.png")).
        shortcut(lit("Ctrl+E")).
        autoRepeat(false);

    factory(QnActions::CameraListAction).
        flags(Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Devices List"),
            tr("Cameras List")
        )).
        shortcut(lit("Ctrl+M")).
        autoRepeat(false);

    factory(QnActions::MergeSystems).
        flags(Qn::Main | Qn::Tree).
        text(tr("Merge Systems...")).
        condition(new QnConjunctionActionCondition(
            new QnTreeNodeTypeCondition({Qn::CurrentSystemNode, Qn::ServersNode}, this),
            new QnForbiddenInSafeModeCondition(this),
            new QnRequiresOwnerCondition(this),
            this)
        );

    factory().
        flags(Qn::Main).
        separator();

    factory(QnActions::AboutAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("About...")).
        shortcut(lit("F1")).
        shortcutContext(Qt::ApplicationShortcut).
        role(QAction::AboutRole).
        autoRepeat(false);

    factory().
        flags(Qn::Main).
        separator();

    factory(QnActions::ExitAction).
        flags(Qn::Main | Qn::GlobalHotkey).
        text(tr("Exit")).
        shortcut(lit("Alt+F4")).
        shortcut(lit("Ctrl+Q"), QnActionBuilder::Mac, true).
        shortcutContext(Qt::ApplicationShortcut).
        role(QAction::QuitRole).
        autoRepeat(false).
        icon(qnSkin->icon("titlebar/window_close.png")).
        iconVisibleInMenu(false);

    factory(QnActions::DelayedForcedExitAction).
        flags(Qn::NoTarget);

    factory(QnActions::BeforeExitAction).
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
    factory(QnActions::OpenInLayoutAction)
        .flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget)
        .requiredTargetPermissions(Qn::LayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission)
        .text(tr("Open in Layout"))
        .condition(new QnOpenInLayoutActionCondition(this));

    factory(QnActions::OpenInCurrentLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission | Qn::AddRemoveItemsPermission).
        text(tr("Open")).
        conditionalText(tr("Monitor"), hasFlags(Qn::server), Qn::All).
        condition(new QnOpenInCurrentLayoutActionCondition(this));

    factory(QnActions::OpenInNewLayoutAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in New Tab")).
        conditionalText(tr("Monitor in New Tab"), hasFlags(Qn::server), Qn::All).
        condition(new QnOpenInNewEntityActionCondition(this));

    factory(QnActions::OpenInAlarmLayoutAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open in Alarm Layout"));

    factory(QnActions::OpenInNewWindowAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget | Qn::WidgetTarget).
        text(tr("Open in New Window")).
        conditionalText(tr("Monitor in New Window"), hasFlags(Qn::server), Qn::All).
        condition(new QnConjunctionActionCondition(
            new QnOpenInNewEntityActionCondition(this),
            new QnLightModeCondition(Qn::LightModeNoNewWindow, this),
            this));

    factory(QnActions::OpenSingleLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Open Layout in New Tab")).
        condition(hasFlags(Qn::layout));

    factory(QnActions::OpenMultipleLayoutsAction).
        flags(Qn::Tree | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layouts")).
        condition(hasFlags(Qn::layout));

    factory(QnActions::OpenLayoutsInNewWindowAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layout(s) in New Window")). // TODO: #Elric split into sinle- & multi- action
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::layout), Qn::All, this),
            new QnLightModeCondition(Qn::LightModeNoNewWindow, this),
            this));

    factory(QnActions::OpenCurrentLayoutInNewWindowAction).
        flags(Qn::NoTarget).
        text(tr("Open Current Layout in New Window")).
        condition(new QnLightModeCondition(Qn::LightModeNoNewWindow, this));

    factory(QnActions::OpenAnyNumberOfLayoutsAction).
        flags(Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Layout(s)")).
        condition(hasFlags(Qn::layout));

    factory(QnActions::OpenVideoWallsReviewAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Open Video Wall(s)")).
        condition(hasFlags(Qn::videowall));

    factory(QnActions::OpenInFolderAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Open Containing Folder")).
        shortcut(lit("Ctrl+Enter")).
        shortcut(lit("Ctrl+Return")).
        autoRepeat(false).
        condition(new QnOpenInFolderActionCondition(this));

    factory(QnActions::IdentifyVideoWallAction).
        flags(Qn::Tree | Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Identify")).
        autoRepeat(false).
        condition(new QnIdentifyVideoWallActionCondition(this));

    factory(QnActions::AttachToVideoWallAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Attach to Video Wall...")).
        autoRepeat(false).
        condition(new QnConjunctionActionCondition(
            new QnForbiddenInSafeModeCondition(this),
            new QnResourceActionCondition(hasFlags(Qn::videowall), Qn::Any, this),
            this));

    factory(QnActions::StartVideoWallAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Switch to Video Wall mode...")).
        autoRepeat(false).
        condition(new QnStartVideowallActionCondition(this));

    factory(QnActions::SaveVideoWallReviewAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Save Video Wall View")).
        shortcut(lit("Ctrl+S")).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        autoRepeat(false).
        condition(new QnConjunctionActionCondition(
            new QnSaveVideowallReviewActionCondition(false, this),
            new QnForbiddenInSafeModeCondition(this),
            this));

    factory(QnActions::SaveVideowallMatrixAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Save Current Matrix")).
        autoRepeat(false).
        condition(new QnConjunctionActionCondition(
            new QnNonEmptyVideowallActionCondition(this),
            new QnForbiddenInSafeModeCondition(this),
            this));

    factory(QnActions::LoadVideowallMatrixAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::VideoWallMatrixTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        condition(new QnForbiddenInSafeModeCondition(this)).
        text(tr("Load Matrix"));

    factory(QnActions::DeleteVideowallMatrixAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallMatrixTarget | Qn::IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Delete")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, QnActionBuilder::Mac, true).
        condition(new QnForbiddenInSafeModeCondition(this)).
        autoRepeat(false);

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(QnActions::StopVideoWallAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Stop Video Wall")).
        autoRepeat(false).
        condition(new QnRunningVideowallActionCondition(this));

    factory(QnActions::ClearVideoWallScreen).
        flags(Qn::Tree | Qn::VideoWallReviewScene | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Clear Screen")).
        autoRepeat(false).
        condition(new QnDetachFromVideoWallActionCondition(this));

    factory(QnActions::SaveLayoutAction).
        flags(Qn::TitleBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        requiredTargetPermissions(Qn::SavePermission).
        text(tr("Save Layout")).
        condition(new QnSaveLayoutActionCondition(false, this));

    factory(QnActions::SaveLayoutAsAction).
        flags(Qn::SingleTarget | Qn::ResourceTarget).
        requiredTargetPermissions(Qn::UserResourceRole, Qn::SavePermission).    //TODO: #GDM #access check canCreateResource permission
        text(lit("If you see this string, notify me. #GDM")).
        condition(new QnSaveLayoutAsActionCondition(false, this));

    factory(QnActions::SaveLayoutForCurrentUserAsAction).
        flags(Qn::TitleBar | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Save Layout As...")).
        condition(new QnSaveLayoutAsActionCondition(false, this));

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(QnActions::DeleteVideoWallItemAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::VideoWallItemTarget | Qn::IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Delete")).
        condition(new QnForbiddenInSafeModeCondition(this)).
        autoRepeat(false);

    factory(QnActions::MaximizeItemAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Maximize Item")).
        shortcut(lit("Enter")).
        shortcut(lit("Return")).
        autoRepeat(false).
        condition(new QnItemZoomedActionCondition(false, this));

    factory(QnActions::UnmaximizeItemAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Restore Item")).
        shortcut(lit("Enter")).
        shortcut(lit("Return")).
        autoRepeat(false).
        condition(new QnItemZoomedActionCondition(true, this));

    factory(QnActions::ShowInfoAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Show Info")).
        shortcut(lit("Alt+I")).
        condition(new QnDisplayInfoActionCondition(false, this));

    factory(QnActions::HideInfoAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Hide Info")).
        shortcut(lit("Alt+I")).
        condition(new QnDisplayInfoActionCondition(true, this));

    factory(QnActions::ToggleInfoAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::HotkeyOnly).
        text(tr("Toggle Info")).
        shortcut(lit("Alt+I")).
        condition(new QnDisplayInfoActionCondition(this));

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Change Resolution...")).
        condition(new QnChangeResolutionActionCondition(this));

    factory.beginSubMenu();
    {
        factory.beginGroup();
        factory(QnActions::RadassAutoAction).
            flags(Qn::Scene | Qn::NoTarget).
            text(tr("Auto")).
            checkable().
            checked();

        factory(QnActions::RadassLowAction).
            flags(Qn::Scene | Qn::NoTarget).
            text(tr("Low")).
            checkable();

        factory(QnActions::RadassHighAction).
            flags(Qn::Scene | Qn::NoTarget).
            text(tr("High")).
            checkable();
        factory.endGroup();
    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::SingleTarget).
        childFactory(new QnPtzPresetsToursActionFactory(this)).
        text(tr("PTZ...")).
        requiredTargetPermissions(Qn::WritePtzPermission).
        condition(new QnPtzActionCondition(Qn::PresetsPtzCapability, false, this));

    factory.beginSubMenu();
    {

        factory(QnActions::PtzSavePresetAction).
            mode(QnActionTypes::DesktopMode).
            flags(Qn::Scene | Qn::SingleTarget).
            text(tr("Save Current Position...")).
            requiredTargetPermissions(Qn::WritePtzPermission | Qn::SavePermission).
            condition(new QnPtzActionCondition(Qn::PresetsPtzCapability, true, this));

        factory(QnActions::PtzManageAction).
            mode(QnActionTypes::DesktopMode).
            flags(Qn::Scene | Qn::SingleTarget).
            text(tr("Manage...")).
            requiredTargetPermissions(Qn::WritePtzPermission | Qn::SavePermission).
            condition(new QnPtzActionCondition(Qn::ToursPtzCapability, false, this));

    } factory.endSubMenu();

    factory(QnActions::PtzCalibrateFisheyeAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Calibrate Fisheye")).
        condition(new QnPtzActionCondition(Qn::VirtualPtzCapability, false, this));

#if 0
    factory(QnActions::ToggleRadassAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::HotkeyOnly).
        text(tr("Toggle Resolution Mode")).
        shortcut(lit("Alt+I")).
        condition(new QnDisplayInfoActionCondition(this));
#endif

    factory(QnActions::StartSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Show Motion/Smart Search")).
        conditionalText(tr("Show Motion"), new QnNoArchiveActionCondition(this)).
        shortcut(lit("Alt+G")).
        condition(new QnSmartSearchActionCondition(false, this));

    // TODO: #ynikitenkov remove this action, use StartSmartSearchAction with checked state!
    factory(QnActions::StopSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Hide Motion/Smart Search")).
        conditionalText(tr("Hide Motion"), new QnNoArchiveActionCondition(this)).
        shortcut(lit("Alt+G")).
        condition(new QnSmartSearchActionCondition(true, this));

    factory(QnActions::ClearMotionSelectionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Clear Motion Selection")).
        condition(new QnClearMotionSelectionActionCondition(this));

    factory(QnActions::ToggleSmartSearchAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::HotkeyOnly).
        text(tr("Toggle Smart Search")).
        shortcut(lit("Alt+G")).
        condition(new QnSmartSearchActionCondition(this));

    factory(QnActions::CheckFileSignatureAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Check File Watermark")).
        shortcut(lit("Alt+C")).
        autoRepeat(false).
        condition(new QnCheckFileSignatureActionCondition(this));

    factory(QnActions::TakeScreenshotAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::HotkeyOnly).
        text(tr("Take Screenshot")).
        shortcut(lit("Alt+S")).
        autoRepeat(false).
        condition(new QnTakeScreenshotActionCondition(this));

    factory(QnActions::AdjustVideoAction).
        flags(Qn::Scene | Qn::SingleTarget).
        text(tr("Image Enhancement...")).
        shortcut(lit("Alt+J")).
        autoRepeat(false).
        condition(new QnAdjustVideoActionCondition(this));

    factory(QnActions::CreateZoomWindowAction).
        flags(Qn::SingleTarget | Qn::WidgetTarget).
        text(tr("Create Zoom Window")).
        condition(new QnCreateZoomWindowActionCondition(this));

    factory().
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
        text(tr("Rotate to..."));

    factory.beginSubMenu();
    {
        factory(QnActions::Rotate0Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("0 degrees")).
            condition(new QnRotateItemCondition(this));

        factory(QnActions::Rotate90Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("90 degrees")).
            condition(new QnRotateItemCondition(this));

        factory(QnActions::Rotate180Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("180 degrees")).
            condition(new QnRotateItemCondition(this));

        factory(QnActions::Rotate270Action).
            flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget).
            text(tr("270 degrees")).
            condition(new QnRotateItemCondition(this));
    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(QnActions::RemoveLayoutItemAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::LayoutItemTarget | Qn::IntentionallyAmbiguous).
        text(tr("Remove from Layout")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, QnActionBuilder::Mac, true).
        autoRepeat(false).
        condition(new QnLayoutItemRemovalActionCondition(this));

    factory(QnActions::RemoveLayoutItemFromSceneAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::MultiTarget | Qn::LayoutItemTarget | Qn::IntentionallyAmbiguous).
        text(tr("Remove from Layout")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, QnActionBuilder::Mac, true).
        autoRepeat(false).
        condition(new QnLayoutItemRemovalActionCondition(this));

    factory(QnActions::RemoveFromServerAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::IntentionallyAmbiguous).
        requiredTargetPermissions(Qn::RemovePermission).
        text(tr("Delete")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, QnActionBuilder::Mac, true).
        autoRepeat(false).
        condition(new QnResourceRemovalActionCondition(this));

    factory(QnActions::StopSharingLayoutAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        text(tr("Stop Sharing Layout")).
        shortcut(lit("Del")).
        shortcut(Qt::Key_Backspace, QnActionBuilder::Mac, true).
        autoRepeat(false).
        condition(new QnStopSharingActionCondition(this));

    factory().
        flags(Qn::Scene | Qn::Tree).
        separator();

    factory(QnActions::RenameResourceAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::IntentionallyAmbiguous).
        requiredTargetPermissions(Qn::WritePermission | Qn::WriteNamePermission).
        text(tr("Rename")).
        shortcut(lit("F2")).
        autoRepeat(false).
        condition(new QnRenameResourceActionCondition(this));

    factory(QnActions::RenameVideowallEntityAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::VideoWallItemTarget | Qn::VideoWallMatrixTarget | Qn::IntentionallyAmbiguous).
        requiredGlobalPermission(Qn::GlobalControlVideoWallPermission).
        text(tr("Rename")).
        shortcut(lit("F2")).
        condition(new QnForbiddenInSafeModeCondition(this)).
        autoRepeat(false);

    factory().
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        separator();

    //TODO: #gdm restore this functionality and allow to delete exported layouts
    factory(QnActions::DeleteFromDiskAction).
        //flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Delete from Disk")).
        autoRepeat(false).
        condition(hasFlags(Qn::local_media));

    factory(QnActions::SetAsBackgroundAction).
        flags(Qn::Scene | Qn::SingleTarget).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission).
        text(tr("Set as Layout Background")).
        autoRepeat(false).
        condition(new QnConjunctionActionCondition(
            new QnSetAsBackgroundActionCondition(this),
            new QnLightModeCondition(Qn::LightModeNoLayoutBackground, this),
            new QnForbiddenInSafeModeCondition(this),
            this));

    factory(QnActions::UserSettingsAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("User Settings...")).
        requiredTargetPermissions(Qn::ReadPermission).
        condition(hasFlags(Qn::user));

    factory(QnActions::UserRolesAction).
        flags(Qn::Tree | Qn::NoTarget).
        text(tr("User Roles...")).
        conditionalText(tr("Role Settings..."), new QnTreeNodeTypeCondition(Qn::RoleNode, this)).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnDisjunctionActionCondition(
            new QnTreeNodeTypeCondition(Qn::UsersNode, this),
            new QnTreeNodeTypeCondition(Qn::RoleNode, this),
            this));

    factory(QnActions::CameraIssuesAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        dynamicText(new QnDevicesNameActionTextFactory(
            QnCameraDeviceStringSet(
                tr("Check Device Issues..."), tr("Check Devices Issues..."),
                tr("Check Camera Issues..."), tr("Check Cameras Issues..."),
                tr("Check I/O Module Issues..."), tr("Check I/O Modules Issues...")
            ), this)).
        requiredGlobalPermission(Qn::GlobalViewLogsPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::Any, this),
            new QnPreviewSearchModeCondition(true, this),
            this));

    factory(QnActions::CameraBusinessRulesAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        dynamicText(new QnDevicesNameActionTextFactory(
            QnCameraDeviceStringSet(
                tr("Device Rules..."), tr("Devices Rules..."),
                tr("Camera Rules..."), tr("Cameras Rules..."),
                tr("I/O Module Rules..."), tr("I/O Modules Rules...")
            ), this)).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::ExactlyOne, this),
            new QnPreviewSearchModeCondition(true, this),
            this));

    factory(QnActions::CameraSettingsAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        dynamicText(new QnDevicesNameActionTextFactory(
            QnCameraDeviceStringSet(
                tr("Device Settings..."), tr("Devices Settings..."),
                tr("Camera Settings..."), tr("Cameras Settings..."),
                tr("I/O Module Settings..."), tr("I/O Modules Settings...")
            ), this)).
        requiredGlobalPermission(Qn::GlobalEditCamerasPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::live_cam), Qn::Any, this),
            new QnPreviewSearchModeCondition(true, this),
            this));

    factory(QnActions::MediaFileSettingsAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("File Settings...")).
        condition(new QnResourceActionCondition(hasFlags(Qn::local_media), Qn::Any, this));

    factory(QnActions::LayoutSettingsAction).
        mode(QnActionTypes::DesktopMode).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Layout Settings...")).
        requiredTargetPermissions(Qn::EditLayoutSettingsPermission).
        condition(new QnLightModeCondition(Qn::LightModeNoLayoutBackground, this));

    factory(QnActions::VideowallSettingsAction).
        flags(Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Video Wall Settings...")).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::videowall), Qn::ExactlyOne, this),
            new QnAutoStartAllowedActionCodition(this),
            new QnForbiddenInSafeModeCondition(this),
            this));

    factory(QnActions::ServerAddCameraManuallyAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Add Device...")).   //intentionally hardcode devices here
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
            new QnEdgeServerCondition(false, this),
            new QnNegativeActionCondition(new QnFakeServerActionCondition(true, this), this),
            new QnForbiddenInSafeModeCondition(this),
            this));

    factory(QnActions::CameraListByServerAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Devices List by Server..."),
            tr("Cameras List by Server...")
        )).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
            new QnEdgeServerCondition(false, this),
            new QnNegativeActionCondition(new QnFakeServerActionCondition(true, this), this),
            this));

    factory(QnActions::PingAction).
        flags(Qn::NoTarget).
        text(tr("Ping..."));

    factory(QnActions::ServerLogsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Logs...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
            new QnNegativeActionCondition(new QnFakeServerActionCondition(true, this), this),
            this));

    factory(QnActions::ServerIssuesAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Diagnostics...")).
        requiredGlobalPermission(Qn::GlobalViewLogsPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
            new QnNegativeActionCondition(new QnFakeServerActionCondition(true, this), this),
            this));

    factory(QnActions::WebAdminAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Web Page...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
            new QnNegativeActionCondition(new QnFakeServerActionCondition(true, this), this),
            new QnNegativeActionCondition(new QnCloudServerActionCondition(Qn::Any, this), this),
            this));

    factory(QnActions::ServerSettingsAction).
        flags(Qn::Scene | Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget | Qn::LayoutItemTarget).
        text(tr("Server Settings...")).
        requiredGlobalPermission(Qn::GlobalAdminPermission).
        condition(new QnConjunctionActionCondition(
            new QnResourceActionCondition(hasFlags(Qn::remote_server), Qn::ExactlyOne, this),
            new QnNegativeActionCondition(new QnFakeServerActionCondition(true, this), this),
            this));

    factory(QnActions::ConnectToCurrentSystem).
        flags(Qn::Tree | Qn::SingleTarget | Qn::MultiTarget | Qn::ResourceTarget).
        text(tr("Merge to Currently Connected System...")).
        condition(new QnConjunctionActionCondition(
            new QnTreeNodeTypeCondition(Qn::ResourceNode, this),
            new QnForbiddenInSafeModeCondition(this),
            new QnMergeToCurrentSystemActionCondition(this),
            new QnRequiresOwnerCondition(this),
            this));

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        childFactory(new QnAspectRatioActionFactory(this)).
        text(tr("Change Cell Aspect Ratio...")).
        condition(new QnConjunctionActionCondition(
            new QnVideoWallReviewModeCondition(true, this),
            new QnLightModeCondition(Qn::LightModeSingleItem, this),
            this));

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        text(tr("Change Cell Spacing...")).
        condition(new QnLightModeCondition(Qn::LightModeSingleItem, this));

    factory.beginSubMenu();
    {
        factory.beginGroup();

        factory(QnActions::SetCurrentLayoutItemSpacingNoneAction).
            flags(Qn::Scene | Qn::NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("None")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::None));

        factory(QnActions::SetCurrentLayoutItemSpacingSmallAction).
            flags(Qn::Scene | Qn::NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Small")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Small));

        factory(QnActions::SetCurrentLayoutItemSpacingMediumAction).
            flags(Qn::Scene | Qn::NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Medium")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Medium));

        factory(QnActions::SetCurrentLayoutItemSpacingLargeAction).
            flags(Qn::Scene | Qn::NoTarget).
            requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::WritePermission).
            text(tr("Large")).
            checkable().
            checked(qnGlobals->defaultLayoutCellSpacing()
                == QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing::Large));
        factory.endGroup();

    } factory.endSubMenu();

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        separator();

    factory(QnActions::ToggleTourModeAction).
        flags(Qn::Scene | Qn::NoTarget | Qn::GlobalHotkey).
        mode(QnActionTypes::DesktopMode).
        text(tr("Start Tour")).
        toggledText(tr("Stop Tour")).
        shortcut(lit("Alt+T")).
        autoRepeat(false).
        condition(new QnToggleTourActionCondition(this));

    factory().
        flags(Qn::Scene | Qn::NoTarget).
        separator();

    factory(QnActions::CurrentLayoutSettingsAction).
        flags(Qn::Scene | Qn::NoTarget).
        requiredTargetPermissions(Qn::CurrentLayoutResourceRole, Qn::EditLayoutSettingsPermission).
        text(tr("Layout Settings...")).
        condition(new QnLightModeCondition(Qn::LightModeNoLayoutBackground, this));

    /* Tab bar actions. */
    factory().
        flags(Qn::TitleBar).
        separator();

    factory(QnActions::CloseLayoutAction).
        flags(Qn::TitleBar | Qn::ScopelessHotkey | Qn::SingleTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Close")).
        shortcut(lit("Ctrl+W")).
        autoRepeat(false);

    factory(QnActions::CloseAllButThisLayoutAction).
        flags(Qn::TitleBar | Qn::SingleTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Close All But This")).
        condition(new QnLayoutCountActionCondition(2, this));

    /* Slider actions. */
    factory(QnActions::StartTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Mark Selection Start")).
        shortcut(lit("[")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new QnTimePeriodActionCondition(Qn::NullTimePeriod, Qn::InvisibleAction, this));

    factory(QnActions::EndTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Mark Selection End")).
        shortcut(lit("]")).
        shortcutContext(Qt::WidgetShortcut).
        condition(new QnTimePeriodActionCondition(Qn::EmptyTimePeriod, Qn::InvisibleAction, this));

    factory(QnActions::ClearTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Clear Selection")).
        condition(new QnTimePeriodActionCondition(Qn::EmptyTimePeriod | Qn::NormalTimePeriod, Qn::InvisibleAction, this));

    factory(QnActions::ZoomToTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Zoom to Selection")).
        condition(new QnTimePeriodActionCondition(Qn::NormalTimePeriod, Qn::InvisibleAction, this));

    factory(QnActions::AddCameraBookmarkAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Add Bookmark...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(new QnConjunctionActionCondition(
            new QnForbiddenInSafeModeCondition(this),
            new QnAddBookmarkActionCondition(this),
            this));

    factory(QnActions::EditCameraBookmarkAction).
        flags(Qn::Slider | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Edit Bookmark...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(new QnConjunctionActionCondition(
            new QnForbiddenInSafeModeCondition(this),
            new QnModifyBookmarkActionCondition(this),
            this));

    factory(QnActions::RemoveCameraBookmarkAction).
        flags(Qn::Slider | Qn::SingleTarget).
        text(tr("Remove Bookmark...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(new QnConjunctionActionCondition(
            new QnForbiddenInSafeModeCondition(this),
            new QnModifyBookmarkActionCondition(this),
            this));

    factory(QnActions::RemoveBookmarksAction).
        flags(Qn::NoTarget | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Remove Bookmarks...")).
        requiredGlobalPermission(Qn::GlobalManageBookmarksPermission).
        condition(new QnConjunctionActionCondition(
            new QnForbiddenInSafeModeCondition(this),
            new QnRemoveBookmarksActionCondition(this),
            this));

    factory().
        flags(Qn::Slider | Qn::SingleTarget).
        separator();

    factory(QnActions::ExportTimeSelectionAction).
        flags(Qn::Slider | Qn::SingleTarget | Qn::ResourceTarget).
        text(tr("Export Selected Area...")).
        requiredTargetPermissions(Qn::ExportPermission).
        condition(new QnExportActionCondition(true, this));

    factory(QnActions::ExportLayoutAction).
        flags(Qn::Slider | Qn::SingleTarget | Qn::MultiTarget | Qn::NoTarget).
        text(tr("Export Multi-Video...")).
        requiredTargetPermissions(Qn::CurrentLayoutMediaItemsRole, Qn::ExportPermission).
        condition(new QnExportActionCondition(false, this));

    factory(QnActions::ExportTimelapseAction).
        flags(Qn::Slider | Qn::SingleTarget | Qn::MultiTarget | Qn::NoTarget).
        text(tr("Export Rapid Review...")).
        requiredTargetPermissions(Qn::CurrentLayoutMediaItemsRole, Qn::ExportPermission).
        condition(new QnExportActionCondition(true, this));

    factory(QnActions::ThumbnailsSearchAction).
        flags(Qn::Slider | Qn::Scene | Qn::SingleTarget).
        mode(QnActionTypes::DesktopMode).
        text(tr("Preview Search...")).
        condition(new QnPreviewActionCondition(this));


    factory(QnActions::DebugIncrementCounterAction).
        flags(Qn::GlobalHotkey | Qn::DevMode).
        shortcut(lit("Ctrl+Alt+Shift++")).
        text(lit("Increment Debug Counter"));

    factory(QnActions::DebugDecrementCounterAction).
        flags(Qn::GlobalHotkey | Qn::DevMode).
        shortcut(lit("Ctrl+Alt+Shift+-")).
        text(lit("Decrement Debug Counter"));

    factory(QnActions::DebugCalibratePtzAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::DevMode).
        text(lit("Calibrate PTZ"));

    factory(QnActions::DebugGetPtzPositionAction).
        flags(Qn::Scene | Qn::SingleTarget | Qn::DevMode).
        text(lit("Get PTZ Position"));

    factory(QnActions::DebugControlPanelAction).
        flags(Qn::GlobalHotkey | Qn::DevMode).
        shortcut(lit("Ctrl+Alt+Shift+D")).
        text(lit("Debug Control Panel"));

    factory(QnActions::PlayPauseAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Space")).
        text(tr("Play")).
        toggledText(tr("Pause")).
        condition(new QnArchiveActionCondition(this));

    factory(QnActions::PreviousFrameAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Ctrl+Left")).
        text(tr("Previous Frame")).
        condition(new QnArchiveActionCondition(this));

    factory(QnActions::NextFrameAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Ctrl+Right")).
        text(tr("Next Frame")).
        condition(new QnArchiveActionCondition(this));

    factory(QnActions::JumpToStartAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Z")).
        text(tr("To Start")).
        condition(new QnArchiveActionCondition(this));

    factory(QnActions::JumpToEndAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("X")).
        text(tr("To End")).
        condition(new QnArchiveActionCondition(this));

    factory(QnActions::VolumeUpAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Ctrl+Up")).
        text(tr("Volume Down")).
        condition(new QnTimelineVisibleActionCondition(this));

    factory(QnActions::VolumeDownAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("Ctrl+Down")).
        text(tr("Volume Up")).
        condition(new QnTimelineVisibleActionCondition(this));

    factory(QnActions::ToggleMuteAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("M")).
        text(tr("Toggle Mute")).
        checkable().
        condition(new QnTimelineVisibleActionCondition(this));

    factory(QnActions::JumpToLiveAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("L")).
        text(tr("Jump to Live")).
        checkable().
        condition(new QnArchiveActionCondition(this));

    factory(QnActions::ToggleSyncAction).
        flags(Qn::ScopelessHotkey | Qn::HotkeyOnly | Qn::Slider | Qn::SingleTarget).
        shortcut(lit("S")).
        text(tr("Synchronize Streams")).
        toggledText(tr("Disable Stream Synchronization")).
        condition(new QnArchiveActionCondition(this));


    factory().
        flags(Qn::Slider | Qn::TitleBar | Qn::Tree).
        separator();

    factory(QnActions::ToggleThumbnailsAction).
        flags(Qn::NoTarget).
        text(tr("Show Thumbnails")).
        toggledText(tr("Hide Thumbnails"));

    factory(QnActions::BookmarksModeAction).
        flags(Qn::NoTarget).
        text(tr("Show Bookmarks")).
        requiredGlobalPermission(Qn::GlobalViewBookmarksPermission).
        toggledText(tr("Hide Bookmarks"));

    factory(QnActions::ToggleCalendarAction).
        flags(Qn::NoTarget).
        text(tr("Show Calendar")).
        toggledText(tr("Hide Calendar"));

    factory(QnActions::ToggleTitleBarAction).
        flags(Qn::NoTarget).
        text(tr("Show Title Bar")).
        toggledText(tr("Hide Title Bar")).
        condition(new QnToggleTitleBarActionCondition(this));

    factory(QnActions::PinTreeAction).
        flags(Qn::Tree | Qn::NoTarget).
        text(tr("Pin Tree")).
        toggledText(tr("Unpin Tree")).
        condition(new QnTreeNodeTypeCondition(Qn::RootNode, this));

    factory(QnActions::PinCalendarAction).
        text(tr("Pin Calendar")).
        toggledText(tr("Unpin Calendar"));

    factory(QnActions::MinimizeDayTimeViewAction).
        text(tr("Minimize")).
        icon(qnSkin->icon("titlebar/dropdown.png"));

    factory(QnActions::ToggleTreeAction).
        flags(Qn::NoTarget).
        text(tr("Show Tree")).
        toggledText(tr("Hide Tree")).
        condition(new QnTreeNodeTypeCondition(Qn::RootNode, this));

    factory(QnActions::ToggleTimelineAction).
        flags(Qn::NoTarget).
        text(tr("Show Timeline")).
        toggledText(tr("Hide Timeline"));

    factory(QnActions::ToggleNotificationsAction).
        flags(Qn::NoTarget).
        text(tr("Show Notifications")).
        toggledText(tr("Hide Notifications"));

    factory(QnActions::PinNotificationsAction).
        flags(Qn::Notifications | Qn::NoTarget).
        text(tr("Pin Notifications")).
        toggledText(tr("Unpin Notifications"));

    factory(QnActions::GoToNextItemAction)
        .flags(Qn::NoTarget);

    factory(QnActions::GoToPreviousItemAction)
        .flags(Qn::NoTarget);

    factory(QnActions::ToggleCurrentItemMaximizationStateAction)
        .flags(Qn::NoTarget);

    factory(QnActions::PtzContinuousMoveAction)
        .flags(Qn::NoTarget);

    factory(QnActions::PtzActivatePresetByIndexAction)
        .flags(Qn::NoTarget);

}

QnActionManager::~QnActionManager()
{
    qDeleteAll(m_idByAction.keys());
}

void QnActionManager::setTargetProvider(QnActionTargetProvider *targetProvider)
{
    m_targetProvider = targetProvider;
    m_targetProviderGuard = dynamic_cast<QObject *>(targetProvider);
    if (!m_targetProviderGuard)
        m_targetProviderGuard = this;
}

void QnActionManager::registerAction(QnAction *action)
{
    if (!action)
    {
        qnNullWarning(action);
        return;
    }

    if (m_idByAction.contains(action))
        return; /* Re-registration is allowed. */

    if (m_actionById.contains(action->id()))
    {
        qnWarning("Action with id '%1' is already registered with this action manager.", action->id());
        return;
    }

    m_actionById[action->id()] = action;
    m_idByAction[action] = action->id();

    emit actionRegistered(action->id());
}

void QnActionManager::registerAlias(QnActions::IDType id, QnActions::IDType targetId)
{
    if (id == targetId)
    {
        qnWarning("Action cannot be an alias of itself.");
        return;
    }

    QnAction *action = this->action(id);
    if (action && action->id() == id)
    { /* Note that re-registration with different target is OK. */
        qnWarning("Id '%1' is already taken by non-alias action '%2'.", id, action->text());
        return;
    }

    QnAction *targetAction = this->action(targetId);
    if (!targetAction)
    {
        qnWarning("Action with id '%1' is not registered with this action manager.", targetId);
        return;
    }

    m_actionById[id] = targetAction;
}

QnAction *QnActionManager::action(QnActions::IDType id) const
{
    return m_actionById.value(id, NULL);
}

QList<QnAction *> QnActionManager::actions() const
{
    return m_idByAction.keys();
}

bool QnActionManager::canTrigger(QnActions::IDType id, const QnActionParameters &parameters)
{
    QnAction *action = m_actionById.value(id);
    if (!action)
        return false;

    return action->checkCondition(action->scope(), parameters) == Qn::EnabledAction;
}

void QnActionManager::trigger(QnActions::IDType id, const QnActionParameters &parameters)
{
    QnAction *action = m_actionById.value(id);
    if (action == NULL)
    {
        qnWarning("Invalid action id '%1'.", static_cast<int>(id));
        return;
    }

    if (action->checkCondition(action->scope(), parameters) != Qn::EnabledAction)
    {
        qnWarning("Action '%1' was triggered with a parameter that does not meet the action's requirements.", action->text());
        return;
    }

    QN_SCOPED_VALUE_ROLLBACK(&m_parametersByMenu[NULL], parameters);
    QN_SCOPED_VALUE_ROLLBACK(&m_shortcutAction, action);
    action->trigger();
}

bool QnActionManager::triggerIfPossible(QnActions::IDType id, const QnActionParameters &parameters)
{
    QnAction *action = m_actionById.value(id);
    if (action == NULL)
    {
        qnWarning("Invalid action id '%1'.", static_cast<int>(id));
        return false;
    }

    if (action->checkCondition(action->scope(), parameters) != Qn::EnabledAction)
    {
        return false;
    }

    QN_SCOPED_VALUE_ROLLBACK(&m_parametersByMenu[NULL], parameters);
    QN_SCOPED_VALUE_ROLLBACK(&m_shortcutAction, action);
    action->trigger();
    return true;
}

QMenu* QnActionManager::integrateMenu(QMenu *menu, const QnActionParameters &parameters)
{
    if (!menu)
        return NULL;

    NX_ASSERT(!m_parametersByMenu.contains(menu));
    m_parametersByMenu[menu] = parameters;
    menu->installEventFilter(this);

    connect(menu, &QMenu::aboutToShow, this, [this, menu] { emit menuAboutToShow(menu); });
    connect(menu, &QMenu::aboutToHide, this, [this, menu] { emit menuAboutToHide(menu); });
    connect(menu, &QObject::destroyed, this, &QnActionManager::at_menu_destroyed);

    return menu;
}


QMenu *QnActionManager::newMenu(Qn::ActionScope scope, QWidget *parent, const QnActionParameters &parameters, CreationOptions options)
{
    /*
     * This method is called when we are opening a brand new context menu.
     * Following check will assure that only the latest context menu will be displayed.
     */
    hideAllMenus();

    return newMenu(QnActions::NoAction, scope, parent, parameters, options);
}

QMenu *QnActionManager::newMenu(QnActions::IDType rootId, Qn::ActionScope scope, QWidget *parent, const QnActionParameters &parameters, CreationOptions options)
{
    QnAction *rootAction = rootId == QnActions::NoAction ? m_root : action(rootId);

    QMenu *result = NULL;
    if (!rootAction)
    {
        qnWarning("No action exists for id '%1'.", static_cast<int>(rootId));
    }
    else
    {
        result = newMenuRecursive(rootAction, scope, parameters, parent, options);
        if (!result)
            result = integrateMenu(new QnMenu(parent), parameters);
    }

    return result;
}

QnActionTargetProvider * QnActionManager::targetProvider() const
{
    return m_targetProviderGuard ? m_targetProvider : NULL;
}

void QnActionManager::copyAction(QAction *dst, QnAction *src, bool forwardSignals)
{
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

    if (forwardSignals)
    {
        connect(dst, &QAction::triggered, src, &QAction::trigger);
        connect(dst, &QAction::toggled, src, &QAction::setChecked);
    }
}

QMenu *QnActionManager::newMenuRecursive(const QnAction *parent, Qn::ActionScope scope, const QnActionParameters &parameters, QWidget *parentWidget, CreationOptions options)
{
    if (parent->childFactory())
    {
        QMenu* childMenu = parent->childFactory()->newMenu(parameters, parentWidget);
        if (childMenu && childMenu->isEmpty())
        {
            delete childMenu;
            return NULL;
        }

        /* Do not need to call integrateMenu, it is already integrated. */
        if (childMenu)
            return childMenu;
        /* Otherwise we should continue to main factory actions. */
    }

    QMenu *result = new QnMenu(parentWidget);

    if (!parent->children().isEmpty())
    {
        foreach(QnAction *action, parent->children())
        {
            Qn::ActionVisibility visibility;
            if (action->flags() & Qn::HotkeyOnly)
            {
                visibility = Qn::InvisibleAction;
            }
            else
            {
                visibility = action->checkCondition(scope, parameters);
            }
            if (visibility == Qn::InvisibleAction)
                continue;

            QMenu *menu = newMenuRecursive(action, scope, parameters, parentWidget, options);
            if ((!menu || menu->isEmpty()) && (action->flags() & Qn::RequiresChildren))
                continue;

            QString replacedText;
            if (menu && menu->actions().size() == 1)
            {
                QnAction *menuAction = qnAction(menu->actions()[0]);
                if (menuAction && (menuAction->flags() & Qn::Pullable))
                {
                    delete menu;
                    menu = NULL;

                    action = menuAction;
                    visibility = action->checkCondition(scope, parameters);
                    replacedText = action->pulledText();
                }
            }

            if (menu)
                connect(result, &QObject::destroyed, menu, &QObject::deleteLater);

            if (action->textFactory())
                replacedText = action->textFactory()->text(parameters);

            if (action->hasConditionalTexts())
                replacedText = action->checkConditionalText(parameters);

            QAction *newAction = NULL;
            if (!replacedText.isEmpty() || visibility == Qn::DisabledAction || menu != NULL || (options & DontReuseActions))
            {
                newAction = new QAction(result);
                copyAction(newAction, action);

                newAction->setMenu(menu);
                newAction->setDisabled(visibility == Qn::DisabledAction);
                if (!replacedText.isEmpty())
                    newAction->setText(replacedText);
            }
            else
            {
                newAction = action;
            }

            if (visibility != Qn::InvisibleAction)
                result->addAction(newAction);
        }
    }

    if (parent->childFactory())
    {
        QList<QAction *> actions = parent->childFactory()->newActions(parameters, NULL);

        if (!actions.isEmpty())
        {
            if (!result->isEmpty())
                result->addSeparator();

            foreach(QAction *action, actions)
            {
                action->setParent(result);
                result->addAction(action);
            }
        }
    }

    if (result->isEmpty())
    {
        delete result;
        return NULL;
    }

    return integrateMenu(result, parameters);
}

QnActionParameters QnActionManager::currentParameters(QnAction *action) const
{
    if (m_shortcutAction == action)
        return m_parametersByMenu.value(NULL);

    if (!m_parametersByMenu.contains(m_lastClickedMenu))
    {
        qnWarning("No active menu, no target exists.");
        return QnActionParameters();
    }

    return m_parametersByMenu.value(m_lastClickedMenu);
}

QnActionParameters QnActionManager::currentParameters(QObject *sender) const
{
    if (QnAction *action = checkSender(sender))
        return currentParameters(action);
    return QnActionParameters();
}

void QnActionManager::redirectAction(QMenu *menu, QnActions::IDType sourceId, QAction *targetAction)
{
    redirectActionRecursive(menu, sourceId, targetAction);
}

bool QnActionManager::isMenuVisible() const
{
    for (auto menu: m_parametersByMenu.keys())
    {
        if (menu && menu->isVisible())
            return true;
    }
    return false;
}

bool QnActionManager::redirectActionRecursive(QMenu *menu, QnActions::IDType sourceId, QAction *targetAction)
{
    QList<QAction *> actions = menu->actions();

    foreach(QAction *action, actions)
    {
        QnAction *storedAction = qnAction(action);
        if (storedAction && storedAction->id() == sourceId)
        {
            int index = actions.indexOf(action);
            QAction *before = index + 1 < actions.size() ? actions[index + 1] : NULL;

            menu->removeAction(action);
            if (targetAction != NULL)
            {
                copyAction(targetAction, storedAction, false);
                targetAction->setEnabled(action->isEnabled());
                menu->insertAction(before, targetAction);
            }

            return true;
        }

        if (action->menu())
        {
            if (redirectActionRecursive(action->menu(), sourceId, targetAction))
            {
                if (action->menu()->isEmpty())
                    menu->removeAction(action);

                return true;
            }
        }
    }

    return false;
}

void QnActionManager::at_menu_destroyed(QObject* menuObj)
{
    auto menu = static_cast<QMenu*>(menuObj);
    m_parametersByMenu.remove(menu);
    if (m_lastClickedMenu == menu)
        m_lastClickedMenu = NULL;
}

bool QnActionManager::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type())
    {
        case QEvent::ShortcutOverride:
        case QEvent::MouseButtonRelease:
            break;
        default:
            return false;
    }

    auto menu = qobject_cast<QMenu*>(watched);
    if (!menu)
        return false;

    m_lastClickedMenu = menu;
    return false;
}

void QnActionManager::hideAllMenus()
{
    for (auto menuObject : m_parametersByMenu.keys())
    {
        if (!menuObject)
            continue;
        if (QMenu* menu = qobject_cast<QMenu*>(menuObject))
            menu->hide();
    }
}
