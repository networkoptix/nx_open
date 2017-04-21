#include "action.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QShortcutEvent>
#include <QtWidgets/QGraphicsWidget>

#include <utils/common/warnings.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/desktop/ui/actions/action_conditions.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_display.h> // TODO: this one does not belong here.

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include "action_manager.h"
#include "action_target_provider.h"
#include "action_factories.h"
#include "action_parameter_types.h"

using namespace nx::client::desktop::ui::action;

QnAction::QnAction(QnActions::IDType id, QObject* parent) :
    QAction(parent),
    QnWorkbenchContextAware(parent),
    m_id(id),
    m_flags(0),
    m_mode(AnyMode),
    m_globalPermission(Qn::NoGlobalPermissions),
    m_toolTipMarker(lit("<b></b>"))
{
    setToolTip(m_toolTipMarker);

    connect(this, &QAction::changed, this, &QnAction::updateToolTipSilent);
}

QnAction::~QnAction()
{}

QnActions::IDType QnAction::id() const
{
    return m_id;
}

ActionScopes QnAction::scope() const
{
    return static_cast<ActionScopes>(static_cast<int>(m_flags) & ScopeMask);
}

ActionParameterTypes QnAction::defaultParameterTypes() const
{
    return static_cast<ActionParameterTypes>(static_cast<int>(m_flags) & TargetTypeMask);
}

Qn::Permissions QnAction::requiredTargetPermissions(int target /* = -1*/) const
{
    return m_targetPermissions.value(target);
}

void QnAction::setRequiredTargetPermissions(Qn::Permissions requiredPermissions)
{
    setRequiredTargetPermissions(-1, requiredPermissions);
}

void QnAction::setRequiredTargetPermissions(int target, Qn::Permissions requiredPermissions)
{
    m_targetPermissions[target] = requiredPermissions;
}

void QnAction::setRequiredGlobalPermission(Qn::GlobalPermission requiredPermission)
{
    m_globalPermission = requiredPermission;
}

ClientModes QnAction::mode() const
{
    return m_mode;
}

void QnAction::setFlags(ActionFlags flags)
{
    m_flags = flags;
}

Qn::ButtonAccent QnAction::accent() const
{
    return m_accent;
}

void QnAction::setAccent(Qn::ButtonAccent value)
{
    m_accent = value;
}

const QString & QnAction::normalText() const
{
    return m_normalText;
}

void QnAction::setMode(ClientModes mode)
{
    m_mode = mode;
}

ActionFlags QnAction::flags() const
{
    return m_flags;
}

void QnAction::setNormalText(const QString& normalText)
{
    if (m_normalText == normalText)
        return;

    m_normalText = normalText;

    updateText();
}

const QString & QnAction::toggledText() const
{
    return m_toggledText.isEmpty() ? m_normalText : m_toggledText;
}

void QnAction::setToggledText(const QString& toggledText)
{
    if (m_toggledText == toggledText)
        return;

    m_toggledText = toggledText;

    if (m_toggledText.isEmpty())
        disconnect(this, &QAction::toggled, this, &QnAction::updateText);
    else
        connect(this, &QAction::toggled, this, &QnAction::updateText, Qt::UniqueConnection);

    updateText();
}

const QString& QnAction::pulledText() const
{
    return m_pulledText.isEmpty() ? m_normalText : m_pulledText;
}

void QnAction::setPulledText(const QString& pulledText)
{
    m_pulledText = pulledText;
}

ConditionPtr QnAction::condition() const
{
    return m_condition;
}

void QnAction::setCondition(const ConditionPtr& condition)
{
    m_condition = condition;
}

QnActionFactory* QnAction::childFactory() const
{
    return m_childFactory.data();
}

void QnAction::setChildFactory(QnActionFactory* childFactory)
{
    m_childFactory = childFactory;
}

QnActionTextFactory* QnAction::textFactory() const
{
    return m_textFactory.data();
}

void QnAction::setTextFactory(QnActionTextFactory* textFactory)
{
    m_textFactory = textFactory;
}

const QList<QnAction*>& QnAction::children() const
{
    return m_children;
}

void QnAction::addChild(QnAction* action)
{
    m_children.push_back(action);
}

void QnAction::removeChild(QnAction* action)
{
    m_children.removeOne(action);
}

QString QnAction::defaultToolTipFormat() const
{
    if (shortcuts().empty())
    {
        return lit("%n");
    }
    else
    {
        return lit("%n (<b>%s</b>)");
    }
}

QString QnAction::toolTipFormat() const
{
    return m_toolTipFormat.isEmpty() ? defaultToolTipFormat() : m_toolTipFormat;
}

void QnAction::setToolTipFormat(const QString& toolTipFormat)
{
    if (m_toolTipFormat == toolTipFormat)
        return;

    m_toolTipFormat = toolTipFormat;

    updateToolTip(true);
}

ActionVisibility QnAction::checkCondition(ActionScopes scope, const QnActionParameters& parameters) const
{
    if (!isVisible())
        return InvisibleAction; // TODO: #Elric cheat!

    if (!(this->scope() & scope) && this->scope() != scope)
        return InvisibleAction;

    if (m_flags.testFlag(DevMode) && !qnRuntime->isDevMode())
        return InvisibleAction;

    if (m_globalPermission != Qn::NoGlobalPermissions &&
        !accessController()->hasGlobalPermission(m_globalPermission))
        return InvisibleAction;

    if (qnRuntime->isVideoWallMode() &&
        !m_mode.testFlag(VideoWallMode))
        return InvisibleAction;

    if (qnRuntime->isActiveXMode() &&
        !m_mode.testFlag(ActiveXMode))
        return InvisibleAction;

    int size = parameters.size();

    if (size == 0 && !m_flags.testFlag(NoTarget))
        return InvisibleAction;

    if (size == 1 && !m_flags.testFlag(SingleTarget))
        return InvisibleAction;

    if (size > 1 && !m_flags.testFlag(MultiTarget))
        return InvisibleAction;

    ActionParameterType type = parameters.type();
    if (!defaultParameterTypes().testFlag(type) && size != 0)
        return InvisibleAction;

    if (!m_targetPermissions.empty())
    {
        for (auto pos = m_targetPermissions.cbegin(); pos != m_targetPermissions.cend(); ++pos)
        {
            int key = pos.key();
            Qn::Permissions required = *pos;

            QnResourceList resources;
            if (parameters.hasArgument(key))
            {
                resources = QnActionParameterTypes::resources(parameters.argument(key));
            }
            else if (key == Qn::CurrentLayoutResourceRole)
            {
                if (QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource())
                    resources.push_back(layout);
            }
            else if (key == Qn::CurrentLayoutMediaItemsRole)
            {
                const QnResourceList& resList = QnActionParameterTypes::resources(context()->display()->widgets());
                for (const QnResourcePtr& res : resList)
                {
                    if (res.dynamicCast<QnMediaResource>())
                        resources.push_back(res);
                }
            }

            if (resources.isEmpty() && required > 0)
                return InvisibleAction;

            if ((accessController()->combinedPermissions(resources) & required) != required)
                return InvisibleAction;
        }
    }

    if (m_condition)
    {
        if (parameters.scope() == InvalidScope)
        {
            QnActionParameters scopedParameters(parameters);
            scopedParameters.setScope(scope);
            return m_condition->check(scopedParameters);
        }
        return m_condition->check(parameters);
    }

    return EnabledAction;
}

bool QnAction::event(QEvent* event)
{
    if (event->type() != QEvent::Shortcut)
        return QObject::event(event);

    // Shortcuts
    QShortcutEvent* e = static_cast<QShortcutEvent*>(event);

    if (e->isAmbiguous())
    {
        NX_ASSERT(m_flags.testFlag(IntentionallyAmbiguous),
            lit("Ambiguous shortcut overload: %1.").arg(e->key().toString()));

        QSet<QAction*> actions;
        for (QWidget* widget : associatedWidgets())
        {
            for (QAction* action : widget->actions())
            {
                if (action->shortcuts().contains(e->key()))
                    actions.insert(action);
            }
        }

        for (QGraphicsWidget* widget : associatedGraphicsWidgets())
        {
            for (QAction* action : widget->actions())
            {
                if (action->shortcuts().contains(e->key()))
                    actions.insert(action);
            }
        }

        /* Just processing current action further. */
        actions.remove(this);

        for (QAction* action : actions)
        {
            QShortcutEvent se(e->key(), e->shortcutId(), false);
            QCoreApplication::sendEvent(action, &se);
        }
    }

    QnActionParameters parameters;
    ActionScope scope = static_cast<ActionScope>(static_cast<int>(this->scope()));

    if (QnActionTargetProvider* targetProvider = QnWorkbenchContextAware::menu()->targetProvider())
    {
        if (!flags().testFlag(ScopelessHotkey))
            scope = targetProvider->currentScope();

        if (!flags().testFlag(TargetlessHotkey))
            parameters = targetProvider->currentParameters(scope);
    }

    if (checkCondition(scope, parameters) == EnabledAction)
        QnWorkbenchContextAware::menu()->trigger(id(), parameters);

    /* Skip shortcut to be handled as usual key event. */
    return false;
}

void QnAction::updateText()
{
    setText(isChecked() ? toggledText() : normalText());
}

void QnAction::updateToolTip(bool notify)
{
    if (!toolTip().endsWith(m_toolTipMarker))
        return; /* We have an explicitly set tooltip. */

    /* This slot is the first to be invoked from changed() signal,
     * so we don't want to emit additional changed() signals if we were called from it. */
    bool signalsBlocked = false;
    if (notify)
        signalsBlocked = blockSignals(true);

    QString toolTip = toolTipFormat();

    int nameIndex = toolTip.indexOf(lit("%n"));
    if (nameIndex != -1)
    {
        QString name = !m_pulledText.isEmpty() ? m_pulledText : text();
        toolTip.replace(nameIndex, 2, name);
    }

    int shortcutIndex = toolTip.indexOf(lit("%s"));
    if (shortcutIndex != -1)
        toolTip.replace(shortcutIndex, 2, shortcut().toString(QKeySequence::NativeText));

    setToolTip(toolTip + m_toolTipMarker);

    if (notify)
        blockSignals(signalsBlocked);
}

void QnAction::updateToolTipSilent()
{
    updateToolTip(false);
}

void QnAction::addConditionalText(ConditionPtr condition, const QString& text)
{
    m_conditionalTexts << ConditionalText(condition, text);
}

bool QnAction::hasConditionalTexts()
{
    return !m_conditionalTexts.isEmpty();
}

QString QnAction::checkConditionalText(const QnActionParameters& parameters) const
{
    for (const ConditionalText& conditionalText : m_conditionalTexts)
    {
        if (conditionalText.condition->check(parameters))
            return conditionalText.text;
    }
    return normalText();
}
