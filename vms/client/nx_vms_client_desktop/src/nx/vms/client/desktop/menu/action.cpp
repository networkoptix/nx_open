// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action.h"

#include <QtGui/QShortcutEvent>
#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QWidget>

#include <client/client_runtime_settings.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>

#include "action_manager.h"
#include "action_parameter_types.h"
#include "action_target_provider.h"

namespace nx::vms::client::desktop {
namespace menu {

Action::Action(IDType id, WindowContext* context, QObject* parent) :
    QAction(parent),
    WindowContextAware(context),
    m_id(id),
    m_mode(AnyMode),
    m_globalPermission(GlobalPermission::none),
    m_toolTipMarker(lit("<b></b>"))
{
    setToolTip(m_toolTipMarker);
    setShortcutVisibleInContextMenu(true);
    connect(this, &QAction::changed, this, &Action::updateToolTipSilent);
}

Action::~Action()
{
}

IDType Action::id() const
{
    return m_id;
}

ActionScopes Action::scope() const
{
    return static_cast<ActionScopes>(static_cast<int>(m_flags) & ScopeMask);
}

ActionParameterTypes Action::defaultParameterTypes() const
{
    return static_cast<ActionParameterTypes>(static_cast<int>(m_flags) & TargetTypeMask);
}

Qn::Permissions Action::requiredTargetPermissions(int target /* = -1*/) const
{
    return m_targetPermissions.value(target);
}

void Action::setRequiredTargetPermissions(Qn::Permissions requiredPermissions)
{
    setRequiredTargetPermissions(-1, requiredPermissions);
}

void Action::setRequiredTargetPermissions(int target, Qn::Permissions requiredPermissions)
{
    m_targetPermissions[target] = requiredPermissions;
}

void Action::setRequiredGlobalPermission(GlobalPermission requiredPermission)
{
    m_globalPermission = requiredPermission;
}

bool Action::isPowerUserRequired() const
{
    return m_powerUserRequired;
}

void Action::setPowerUserRequired(bool value)
{
    m_powerUserRequired = value;
}

ClientModes Action::mode() const
{
    return m_mode;
}

void Action::setFlags(ActionFlags flags)
{
    m_flags = flags;
}

Qn::ButtonAccent Action::accent() const
{
    return m_accent;
}

void Action::setAccent(Qn::ButtonAccent value)
{
    m_accent = value;
}

const QString & Action::normalText() const
{
    return m_normalText;
}

void Action::setMode(ClientModes mode)
{
    m_mode = mode;
}

ActionFlags Action::flags() const
{
    return m_flags;
}

void Action::setNormalText(const QString& normalText)
{
    if (m_normalText == normalText)
        return;

    m_normalText = normalText;

    updateText();
}

const QString & Action::toggledText() const
{
    return m_toggledText.isEmpty() ? m_normalText : m_toggledText;
}

void Action::setToggledText(const QString& toggledText)
{
    if (m_toggledText == toggledText)
        return;

    m_toggledText = toggledText;

    if (m_toggledText.isEmpty())
        disconnect(this, &QAction::toggled, this, &Action::updateText);
    else
        connect(this, &QAction::toggled, this, &Action::updateText, Qt::UniqueConnection);

    updateText();
}

const QString& Action::pulledText() const
{
    return m_pulledText.isEmpty() ? m_normalText : m_pulledText;
}

void Action::setPulledText(const QString& pulledText)
{
    m_pulledText = pulledText;
}

bool Action::hasCondition() const
{
     return m_condition ? true : false; //< using explicit operator bool
}

void Action::setCondition(ConditionWrapper&& condition)
{
    m_condition = std::move(condition);
}

FactoryPtr Action::childFactory() const
{
    return m_childFactory;
}

void Action::setChildFactory(const FactoryPtr& childFactory)
{
    m_childFactory = childFactory;
}

TextFactoryPtr Action::textFactory() const
{
    return m_textFactory;
}

void Action::setTextFactory(const TextFactoryPtr& textFactory)
{
    m_textFactory = textFactory;
}

const QList<Action*>& Action::children() const
{
    return m_children;
}

void Action::addChild(Action* action)
{
    m_children.push_back(action);
}

void Action::removeChild(Action* action)
{
    m_children.removeOne(action);
}

QString Action::defaultToolTipFormat() const
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

QString Action::toolTipFormat() const
{
    return m_toolTipFormat.isEmpty() ? defaultToolTipFormat() : m_toolTipFormat;
}

void Action::setToolTipFormat(const QString& toolTipFormat)
{
    if (m_toolTipFormat == toolTipFormat)
        return;

    m_toolTipFormat = toolTipFormat;

    updateToolTip(true);
}

ActionVisibility Action::checkCondition(ActionScopes scope, const Parameters& parameters) const
{
    if (!isVisible())
        return InvisibleAction;

    if (!(this->scope() & scope) && this->scope() != scope)
        return InvisibleAction;

    if (m_flags.testFlag(DevMode) && !ini().developerMode)
        return InvisibleAction;

    if (m_powerUserRequired
        && !system()->accessController()->hasPowerUserPermissions())
    {
        return InvisibleAction;
    }

    if (m_globalPermission != GlobalPermission::none &&
        !system()->accessController()->hasGlobalPermissions(m_globalPermission))
    {
        return InvisibleAction;
    }

    if (qnRuntime->isVideoWallMode() && !m_mode.testFlag(VideoWallMode))
        return InvisibleAction;

    if (qnRuntime->isAcsMode() && !m_mode.testFlag(AcsMode))
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
                resources = ParameterTypes::resources(parameters.argument(key));
            }
            else if (key == Qn::VideoWallResourceRole)
            {
                resources = ParameterTypes::videowalls(parameters.argument(-1));
            }
            else if (key == Qn::CurrentLayoutResourceRole)
            {
                if (LayoutResourcePtr layout = workbench()->currentLayoutResource())
                    resources.push_back(layout);
            }

            if (resources.isEmpty() && required > 0)
                return InvisibleAction;

            const bool hasPermissions = std::all_of(resources.cbegin(), resources.cend(),
                [required](const QnResourcePtr& resource)
                {
                    return ResourceAccessManager::hasPermissions(resource, required);
                });

            if (!hasPermissions)
                return InvisibleAction;
        }
    }

    if (hasCondition())
    {
        if (parameters.scope() == InvalidScope)
        {
            Parameters scopedParameters(parameters);
            scopedParameters.setScope(scope);
            return m_condition->check(scopedParameters, windowContext());
        }
        return m_condition->check(parameters, windowContext());
    }

    return EnabledAction;
}

bool Action::event(QEvent* event)
{
    if (event->type() != QEvent::Shortcut)
        return QAction::event(event);

    QSet<Action*> actions;
    actions.insert(this);

    QShortcutEvent* e = static_cast<QShortcutEvent*>(event);
    if (e->isAmbiguous())
    {
        const auto toSet =
            [](const QList<QAction*>& list)
            {
                return QSet<QAction*>(list.cbegin(), list.cend());
            };

        QSet<QAction*> associatedActions;

        for (auto object: associatedObjects())
        {
            if (auto widget = qobject_cast<QWidget*>(object))
                associatedActions += toSet(widget->actions());
            else if (auto graphicsWidget = qobject_cast<QGraphicsWidget*>(object))
                associatedActions += toSet(graphicsWidget->actions());
        }

        bool hasRawActions = false;
        for (const auto associatedAction: associatedActions)
        {
            auto action = qobject_cast<Action*>(associatedAction);
            if (!action)
                hasRawActions = true;
            else if (action->shortcuts().contains(e->key()))
                actions.insert(action);
        }

        NX_ASSERT(actions.size() > 1 && !hasRawActions
            && std::all_of(actions.cbegin(), actions.cend(),
                [](Action* action) { return action->flags().testFlag(IntentionallyAmbiguous); }),
            "Ambiguous shortcut overload: %1.", e->key().toString());
    }

    for (auto action: actions)
    {
        Parameters parameters;
        ActionScope scope = static_cast<ActionScope>(static_cast<int>(action->scope()));

        if (auto targetProvider = WindowContextAware::menu()->targetProvider())
        {
            if (!action->flags().testFlag(ScopelessHotkey))
                scope = targetProvider->currentScope();

            if (!action->flags().testFlag(TargetlessHotkey))
                parameters = targetProvider->currentParameters(scope);
        }

        if (action->checkCondition(scope, parameters) == EnabledAction)
        {
            WindowContextAware::menu()->trigger(action->id(), parameters);
            break;
        }
    }

    // Qt shortcuts handling works incorrect. If we have a shortcut set for an action, it will
    // block key event passing in any case (even if we did not handle the event).
    return false;
}

void Action::updateText()
{
    setText(isChecked() ? toggledText() : normalText());
}

void Action::updateToolTip(bool notify)
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

void Action::updateToolTipSilent()
{
    updateToolTip(false);
}

void Action::addConditionalText(ConditionWrapper&& condition, const QString& text)
{
    m_conditionalTexts.emplace_back(std::move(condition), text);
}

bool Action::hasConditionalTexts() const
{
    return !m_conditionalTexts.empty();
}

QString Action::checkConditionalText(const Parameters& parameters) const
{
    for (const auto& conditionalText: m_conditionalTexts)
    {
        if (conditionalText.condition->check(parameters, windowContext()))
            return conditionalText.text;
    }
    return QString();
}

Action::ConditionalText::ConditionalText(ConditionWrapper&& condition, const QString &text):
    condition(std::move(condition)),
    text(text)
{
}

Action::ConditionalText::ConditionalText(ConditionalText&& conditionalText):
    condition(std::move(conditionalText.condition)),
    text(conditionalText.text)
{
}

Action::ConditionalText::~ConditionalText()
{
}

} // namespace menu
} // namespace nx::vms::client::desktop
