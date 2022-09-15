// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_manager.h"

#include <QtCore/QEvent>
#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>

#include <nx/fusion/model_functions.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_builder.h>
#include <nx/vms/client/desktop/ui/actions/action_factories.h>
#include <nx/vms/client/desktop/ui/actions/action_parameter_types.h>
#include <nx/vms/client/desktop/ui/actions/action_target_provider.h>
#include <nx/vms/client/desktop/ui/actions/action_text_factories.h>
#include <nx/vms/client/desktop/ui/actions/menu_factory.h>
#include <nx/vms/client/desktop/workbench/workbench.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

namespace {

const char* sourceActionPropertyName = "_qn_sourceAction";

Action* qnAction(QAction* action)
{
    if (auto source = action->property(sourceActionPropertyName).value<Action*>())
        return source;

    return dynamic_cast<Action*>(action);
}

class QnMenu: public QMenu
{
    typedef QMenu base_type;
public:
    explicit QnMenu(QWidget* parent = 0): base_type(parent) {}

protected:
    virtual void mousePressEvent(QMouseEvent* event) override
    {
        /* This prevents the click from propagating to the underlying widget. */
        setAttribute(Qt::WA_NoMouseReplay);
        base_type::mousePressEvent(event);
    }
};

Action* checkSender(QObject* sender)
{
    auto result = qobject_cast<Action*>(sender);
    NX_ASSERT(result, "Cause cannot be determined for non-Action senders.");
    return result;
}

} // namespace


Manager::Manager(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_root(nullptr),
    m_targetProvider(nullptr),
    m_shortcutAction(nullptr),
    m_lastClickedMenu(nullptr)
{
    m_root = new Action(NoAction, this);
    m_actionById[NoAction] = m_root;
    m_idByAction[m_root] = NoAction;

    connect(workbench(), &Workbench::currentLayoutAboutToBeChanged, this,
        &Manager::hideAllMenus);
    initialize(this, m_root);
}

Manager::~Manager()
{
    qDeleteAll(m_idByAction.keys());
}

void Manager::setTargetProvider(TargetProvider* targetProvider)
{
    m_targetProvider = targetProvider;
    m_targetProviderGuard = dynamic_cast<QObject*>(targetProvider);
    if (!m_targetProviderGuard)
        m_targetProviderGuard = this;
}

void Manager::registerAction(Action* action)
{
    NX_ASSERT(action);
    if (!action)
    {
        return;
    }

    if (m_idByAction.contains(action))
        return; /* Re-registration is allowed. */

    if (m_actionById.contains(action->id()))
    {
        NX_ASSERT(false, nx::format("Action with id '%1' is already registered with this action manager.")
            .arg(action->id()));
        return;
    }

    m_actionById[action->id()] = action;
    m_idByAction[action] = action->id();

    connect(action, &QAction::triggered, this,
        [this, id = action->id()]
        {
            NX_DEBUG(this, "Triggered action %1 with parameters %2",
                id,
                currentParameters(sender()));
        });

    emit actionRegistered(action->id());
}

void Manager::registerAlias(IDType id, IDType targetId)
{
    if (id == targetId)
    {
        NX_ASSERT(false, "Action cannot be an alias of itself.");
        return;
    }

    Action* action = this->action(id);
    if (action && action->id() == id)
    {
        // Note that re-registration with different target is OK.
        NX_ASSERT(false, nx::format("Id '%1' is already taken by non-alias action '%2'.")
            .args(id, action->text()));
        return;
    }

    Action* targetAction = this->action(targetId);
    if (!targetAction)
    {
        NX_ASSERT(false, nx::format("Action with id '%1' is not registered with this action manager.")
            .arg(targetId));
        return;
    }

    m_actionById[id] = targetAction;
}

Action* Manager::action(IDType id) const
{
    return m_actionById.value(id, nullptr);
}

QList<Action*> Manager::actions() const
{
    return m_idByAction.keys();
}

bool Manager::canTrigger(IDType id, const Parameters& parameters)
{
    Action* action = m_actionById.value(id);
    if (!action)
        return false;

    return action->checkCondition(action->scope(), parameters) == EnabledAction;
}

void Manager::trigger(IDType id, const Parameters& parameters)
{
    if (triggerIfPossible(id, parameters))
        return;

    NX_WARNING(this,
        "Action %1 was triggered with a parameter that does not meet its requirements:\n%2",
        id,
        parameters);
}

bool Manager::triggerIfPossible(IDType id, const Parameters& parameters)
{
    Action* action = m_actionById.value(id);
    NX_ASSERT(action);
    if (!action)
        return false;

    if (action->checkCondition(action->scope(), parameters) != EnabledAction)
        return false;

    QScopedValueRollback<Parameters> currentParameters(m_parametersByMenu[nullptr], parameters);
    QScopedValueRollback<Action*> shortcut(m_shortcutAction, action);
    action->trigger();
    return true;
}

void Manager::triggerForced(IDType id, const Parameters& parameters)
{
    Action* action = m_actionById.value(id);
    NX_ASSERT(action);
    if (!action)
        return;

    QScopedValueRollback<Parameters> currentParameters(m_parametersByMenu[nullptr], parameters);
    QScopedValueRollback<Action*> shortcut(m_shortcutAction, action);
    action->trigger();
}

QMenu* Manager::integrateMenu(QMenu* menu, const Parameters& parameters)
{
    if (!menu)
        return nullptr;

    NX_ASSERT(!m_parametersByMenu.contains(menu));
    m_parametersByMenu[menu] = parameters;
    menu->installEventFilter(this);

    connect(menu, &QMenu::aboutToShow, this, [this, menu] { emit menuAboutToShow(menu); });
    connect(menu, &QMenu::aboutToHide, this, [this, menu] { emit menuAboutToHide(menu); });
    connect(menu, &QObject::destroyed, this, &Manager::at_menu_destroyed);

    return menu;
}


QMenu* Manager::newMenu(
    ActionScope scope,
    QWidget* parent,
    const Parameters& parameters,
    CreationOptions options,
    const QSet<IDType>& actionIds)
{
    /*
     * This method is called when we are opening a brand new context menu.
     * Following check will assure that only the latest context menu will be displayed.
     */
    hideAllMenus();

    return newMenu(NoAction, scope, parent, parameters, options, actionIds);
}

QMenu* Manager::newMenu(
    IDType rootId,
    ActionScope scope,
    QWidget* parent,
    const Parameters& parameters,
    CreationOptions options,
    const QSet<IDType>& actionIds)
{
    Action* rootAction = rootId == NoAction ? m_root : action(rootId);
    NX_ASSERT(rootAction);
    if (!rootAction)
        return nullptr;

    auto result = newMenuRecursive(rootAction, scope, parameters, parent, options, actionIds);
    if (!result)
        result = integrateMenu(new QnMenu(parent), parameters);
    return result;
}

TargetProvider* Manager::targetProvider() const
{
    return m_targetProviderGuard ? m_targetProvider : nullptr;
}

void Manager::copyAction(QAction* dst, Action* src, bool forwardSignals)
{
    dst->setText(src->text());
    dst->setIcon(src->icon());
    dst->setShortcuts(src->shortcuts());
    dst->setShortcutVisibleInContextMenu(src->isShortcutVisibleInContextMenu());
    dst->setCheckable(src->isCheckable());
    dst->setChecked(src->isChecked());
    dst->setFont(src->font());
    dst->setIconText(src->iconText());
    dst->setSeparator(src->isSeparator());

    dst->setProperty(sourceActionPropertyName, QVariant::fromValue<Action*>(src));
    foreach(const QByteArray& name, src->dynamicPropertyNames())
        dst->setProperty(name.data(), src->property(name.data()));

    if (forwardSignals)
    {
        if (src->isCheckable())
            connect(dst, &QAction::toggled, src, &QAction::setChecked);
        else
            connect(dst, &QAction::triggered, src, &QAction::trigger);
    }
}

QMenu* Manager::newMenuRecursive(
    const Action* parent,
    ActionScope scope,
    const Parameters& parameters,
    QWidget* parentWidget,
    CreationOptions options,
    const QSet<IDType>& actionIds)
{
    // If the root node has its own childFactory that makes a menu, use it and return.
    if (parent->childFactory())
    {
        QMenu* childMenu = parent->childFactory()->newMenu(parameters, parentWidget);
        if (childMenu && childMenu->isEmpty())
        {
            delete childMenu;
            return nullptr;
        }

        /* Do not need to call integrateMenu, it is already integrated. */
        if (childMenu)
            return childMenu;
        /* Otherwise we should continue to main factory actions. */
    }

    auto result = new QnMenu(parentWidget);

    if (!parent->children().isEmpty())
    {
        foreach(Action* action, parent->children())
        {
            // Skip actions not from actionIds set (if it exists).
            if (!actionIds.empty() && !actionIds.contains(action->id()))
                continue;

            ActionVisibility visibility;
            if (action->flags() & HotkeyOnly)
            {
                visibility = InvisibleAction;
            }
            else
            {
                visibility = action->checkCondition(scope, parameters);
            }
            if (visibility == InvisibleAction)
                continue;

            auto menu = newMenuRecursive(action, scope, parameters, parentWidget, options, actionIds);
            if ((!menu || menu->isEmpty()) && (action->flags() & RequiresChildren))
                continue;

            QString replacedText;
            if (menu && menu->actions().size() == 1)
            {
                Action* menuAction = qnAction(menu->actions()[0]);
                if (menuAction && (menuAction->flags() & Pullable))
                {
                    delete menu;
                    menu = nullptr;

                    action = menuAction;
                    visibility = action->checkCondition(scope, parameters);
                    replacedText = action->pulledText();
                }
            }

            if (menu)
                connect(result, &QObject::destroyed, menu, &QObject::deleteLater);

            if (action->textFactory())
                replacedText = action->textFactory()->text(parameters, context());

            if (action->hasConditionalTexts())
                replacedText = action->checkConditionalText(parameters);

            QAction* newAction = nullptr;
            // Create QAction from the Action if needs to modify anything or by request.
            if (!replacedText.isEmpty() || visibility == DisabledAction
                || menu != nullptr || (options & DontReuseActions))
            {
                newAction = new QAction(result);
                copyAction(newAction, action);

                newAction->setMenu(menu);
                newAction->setDisabled(visibility == DisabledAction);
                if (!replacedText.isEmpty())
                    newAction->setText(replacedText);
            }
            else
            {
                newAction = action;
            }

            if (visibility != InvisibleAction)
                result->addAction(newAction);
        }
    }

    // Add plain (non-menu) items from childFactory if any.
    if (parent->childFactory())
    {
        QList<QAction*> actions = parent->childFactory()->newActions(parameters, result);

        if (!actions.isEmpty())
        {
            if (!result->isEmpty())
                result->addSeparator();

            for(auto action: actions)
            {
                action->setParent(result);
                result->addAction(action);
            }
        }
    }

    if (result->isEmpty())
    {
        delete result;
        return nullptr;
    }

    return integrateMenu(result, parameters);
}

Parameters Manager::currentParameters(Action* action) const
{
    if (m_shortcutAction == action)
        return m_parametersByMenu.value(nullptr);

    return m_parametersByMenu.value(m_lastClickedMenu, Parameters());
}

Parameters Manager::currentParameters(QObject* sender) const
{
    if (Action* action = checkSender(sender))
        return currentParameters(action);
    return Parameters();
}

void Manager::redirectAction(QMenu* menu, IDType sourceId, QAction* targetAction)
{
    redirectActionRecursive(menu, sourceId, targetAction);
}

bool Manager::isMenuVisible() const
{
    for (auto menu: m_parametersByMenu.keys())
    {
        if (menu && menu->isVisible())
            return true;
    }
    return false;
}

bool Manager::redirectActionRecursive(QMenu* menu, IDType sourceId, QAction* targetAction)
{
    QList<QAction*> actions = menu->actions();

    foreach(QAction* action, actions)
    {
        Action* storedAction = qnAction(action);
        if (storedAction && storedAction->id() == sourceId)
        {
            int index = actions.indexOf(action);
            QAction* before = index + 1 < actions.size() ? actions[index + 1] : nullptr;

            menu->removeAction(action);
            if (targetAction != nullptr)
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

void Manager::at_menu_destroyed(QObject* menuObj)
{
    auto menu = static_cast<QMenu*>(menuObj);
    m_parametersByMenu.remove(menu);
    if (m_lastClickedMenu == menu)
        m_lastClickedMenu = nullptr;
}

bool Manager::eventFilter(QObject* watched, QEvent* event)
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

void Manager::hideAllMenus()
{
    for (auto menuObject : m_parametersByMenu.keys())
    {
        if (!menuObject)
            continue;
        if (auto menu = qobject_cast<QMenu*>(menuObject))
            menu->hide();
    }
}

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
