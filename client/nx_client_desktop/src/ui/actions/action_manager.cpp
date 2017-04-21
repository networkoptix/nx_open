#include "action_manager.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>

#include "action.h"

#include "action_target_provider.h"
#include "action_parameter_types.h"
#include <nx/client/desktop/ui/actions/menu_factory.h>
#include <nx/client/desktop/ui/actions/action_builder.h>

#include <ui/workbench/workbench.h>

#include <utils/common/scoped_value_rollback.h>

using namespace nx::client::desktop::ui::action;

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

QnAction *checkSender(QObject *sender)
{
    QnAction *result = qobject_cast<QnAction *>(sender);
    if (!result)
        NX_EXPECT(false, "Cause cannot be determined for non-QnAction senders.");
    return result;
}

bool checkType(const QVariant &items)
{
    ActionParameterType type = QnActionParameterTypes::type(items);
    if (type == 0)
    {
        NX_EXPECT(false, lm("Unrecognized action target type '%1'.").arg(items.typeName()));
        return false;
    }

    return true;
}

} // namespace


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

    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this,
        &QnActionManager::hideAllMenus);
    QnActions::initialize(this, m_root);
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
    NX_EXPECT(action);
    if (!action)
    {
        return;
    }

    if (m_idByAction.contains(action))
        return; /* Re-registration is allowed. */

    if (m_actionById.contains(action->id()))
    {
        NX_EXPECT(false, lm("Action with id '%1' is already registered with this action manager.")
            .arg(action->id()));
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
        NX_EXPECT(false, "Action cannot be an alias of itself.");
        return;
    }

    QnAction *action = this->action(id);
    if (action && action->id() == id)
    {
        // Note that re-registration with different target is OK.
        NX_EXPECT(false, lm("Id '%1' is already taken by non-alias action '%2'.")
            .args(id, action->text()));
        return;
    }

    QnAction *targetAction = this->action(targetId);
    if (!targetAction)
    {
        NX_EXPECT(false, lm("Action with id '%1' is not registered with this action manager.")
            .arg(targetId));
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

    return action->checkCondition(action->scope(), parameters) == EnabledAction;
}

void QnActionManager::trigger(QnActions::IDType id, const QnActionParameters &parameters)
{
    if (triggerIfPossible(id, parameters))
        return;

    const auto action = m_actionById.value(id);
    const QString text = action ? action->text() : QString();
    qWarning() << "Action was triggered with a parameter that does not meet the action's requirements."
        << id << text;
}

bool QnActionManager::triggerIfPossible(QnActions::IDType id, const QnActionParameters &parameters)
{
    QnAction *action = m_actionById.value(id);
    NX_EXPECT(action);
    if (!action)
        return false;

    if (action->checkCondition(action->scope(), parameters) != EnabledAction)
        return false;

    QN_SCOPED_VALUE_ROLLBACK(&m_parametersByMenu[NULL], parameters);
    QN_SCOPED_VALUE_ROLLBACK(&m_shortcutAction, action);
    action->trigger();
    return true;
}

QMenu* QnActionManager::integrateMenu(QMenu *menu, const QnActionParameters &parameters)
{
    if (!menu)
        return NULL;

    NX_EXPECT(!m_parametersByMenu.contains(menu));
    m_parametersByMenu[menu] = parameters;
    menu->installEventFilter(this);

    connect(menu, &QMenu::aboutToShow, this, [this, menu] { emit menuAboutToShow(menu); });
    connect(menu, &QMenu::aboutToHide, this, [this, menu] { emit menuAboutToHide(menu); });
    connect(menu, &QObject::destroyed, this, &QnActionManager::at_menu_destroyed);

    return menu;
}


QMenu *QnActionManager::newMenu(nx::client::desktop::ui::action::ActionScope scope, QWidget *parent, const QnActionParameters &parameters, CreationOptions options)
{
    /*
     * This method is called when we are opening a brand new context menu.
     * Following check will assure that only the latest context menu will be displayed.
     */
    hideAllMenus();

    return newMenu(QnActions::NoAction, scope, parent, parameters, options);
}

QMenu* QnActionManager::newMenu(
    QnActions::IDType rootId,
    nx::client::desktop::ui::action::ActionScope scope,
    QWidget* parent,
    const QnActionParameters& parameters,
    CreationOptions options)
{
    QnAction *rootAction = rootId == QnActions::NoAction ? m_root : action(rootId);
    NX_EXPECT(rootAction);
    if (!rootAction)
        return nullptr;

    QMenu* result = newMenuRecursive(rootAction, scope, parameters, parent, options);
    if (!result)
        result = integrateMenu(new QnMenu(parent), parameters);
    return result;
}

QnActionTargetProvider* QnActionManager::targetProvider() const
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

QMenu *QnActionManager::newMenuRecursive(const QnAction *parent, nx::client::desktop::ui::action::ActionScope scope, const QnActionParameters &parameters, QWidget *parentWidget, CreationOptions options)
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

            QMenu *menu = newMenuRecursive(action, scope, parameters, parentWidget, options);
            if ((!menu || menu->isEmpty()) && (action->flags() & RequiresChildren))
                continue;

            QString replacedText;
            if (menu && menu->actions().size() == 1)
            {
                QnAction *menuAction = qnAction(menu->actions()[0]);
                if (menuAction && (menuAction->flags() & Pullable))
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
            if (!replacedText.isEmpty() || visibility == DisabledAction || menu != NULL || (options & DontReuseActions))
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
        NX_EXPECT(false, "No active menu, no target exists.");
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
