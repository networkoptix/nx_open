// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_builder.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QKeySequence>

#include <nx/build_info.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/style/old_style.h>

#include "action.h"
#include "action_conditions.h"

namespace nx::vms::client::desktop {
namespace menu {

Builder::Builder(Action* action):
    m_action(action)
{
    action->setShortcutContext(Qt::WindowShortcut);
    action->setAutoRepeat(false);
}

Action* Builder::action() const
{
    return m_action;
}

Builder Builder::shortcut(
    const QKeySequence& keySequence,
    Platform platform,
    bool replaceExisting)
{
    // This check is required to avoid crashes in the client unit tests
    QGuiApplication* guiApp = qobject_cast<QGuiApplication*>(qApp);
    if (!guiApp)
        return *this;

    if (keySequence.isEmpty())
        return *this;

    auto platformMatch = [platform]
        {
            switch (platform)
            {
                case Windows:   return build_info::isWindows();
                case Linux:     return build_info::isLinux();
                case Mac:       return build_info::isMacOsX();
                default:        return true; //< AllPlatforms
            }
        };

    if (platformMatch())
    {
        QList<QKeySequence> shortcuts = m_action->shortcuts();
        if (replaceExisting)
            shortcuts.clear();
        shortcuts.append(keySequence);
        m_action->setShortcuts(shortcuts);
    }

    return *this;
}

Builder Builder::shortcut(const QString& keySequence)
{
    return shortcut(QKeySequence(keySequence));
}

Builder Builder::shortcut(const QKeySequence& keySequence)
{
    return shortcut(keySequence, AllPlatforms, false);
}

Builder Builder::shortcutContext(Qt::ShortcutContext context)
{
    m_action->setShortcutContext(context);
    return *this;
}

Builder Builder::icon(const QIcon& icon)
{
    m_action->setIcon(icon);
    return *this;
}

Builder Builder::iconVisibleInMenu(bool visible)
{
    m_action->setIconVisibleInMenu(visible);
    return *this;
}

Builder Builder::role(QAction::MenuRole role)
{
    m_action->setMenuRole(role);
    return *this;
}

Builder Builder::autoRepeat(bool autoRepeat)
{
    m_action->setAutoRepeat(autoRepeat);
    return *this;
}

Builder Builder::text(const QString& text)
{
    m_action->setText(text);
    m_action->setNormalText(text);

    return *this;
}

Builder Builder::dynamicText(const TextFactoryPtr& factory)
{
    m_action->setTextFactory(factory);
    return *this;
}

Builder Builder::toggledText(const QString& text)
{
    m_action->setToggledText(text);
    m_action->setCheckable(true);
    showCheckBoxInMenu(false);
    return *this;
}

Builder Builder::pulledText(const QString& text)
{
    m_action->setPulledText(text);
    m_action->setFlags(m_action->flags() | Pullable);
    return *this;
}

Builder Builder::toolTip(const QString& toolTip)
{
    m_action->setToolTip(toolTip);
    return *this;
}

Builder Builder::toolTipFormat(const QString& toolTipFormat)
{
    m_action->setToolTipFormat(toolTipFormat);
    return *this;
}

Builder Builder::flags(ActionFlags flags)
{
    m_action->setFlags(m_action->flags() | flags);

    return *this;
}

Builder Builder::mode(ClientModes mode)
{
    m_action->setMode(mode);
    return *this;
}

Builder Builder::requiredPowerUserPermissions()
{
    m_action->setPowerUserRequired();
    return *this;
}

Builder Builder::requiredGlobalPermission(GlobalPermission permission)
{
    m_action->setRequiredGlobalPermission(permission);

    return *this;
}

Builder Builder::requiredTargetPermissions(int key, Qn::Permissions permissions)
{
    m_action->setRequiredTargetPermissions(key, permissions);
    return *this;
}

Builder Builder::requiredTargetPermissions(Qn::Permissions permissions)
{
    m_action->setRequiredTargetPermissions(permissions);
    return *this;
}

Builder Builder::separator(bool isSeparator)
{
    m_action->setSeparator(isSeparator);
    m_action->setFlags(m_action->flags() | Separator);

    return *this;
}

Builder Builder::conditionalText(const QString& text, ConditionWrapper&& condition)
{
    m_action->addConditionalText(std::move(condition), text);
    return *this;
}

Builder Builder::checkable(bool isCheckable)
{
    m_action->setCheckable(isCheckable);
    return *this;
}

Builder Builder::checked(bool isChecked)
{
    m_action->setChecked(isChecked);
    return *this;
}

Builder Builder::showCheckBoxInMenu(bool show)
{
    m_action->setProperty(OldStyle::kHideCheckBoxInMenu, !show);
    return *this;
}

Builder Builder::accent(Qn::ButtonAccent value)
{
    m_action->setAccent(value);
    return *this;
}

Builder Builder::condition(ConditionWrapper&& condition)
{
    NX_ASSERT(!m_action->hasCondition());
    m_action->setCondition(std::move(condition));
    return *this;
}

Builder Builder::childFactory(const FactoryPtr& childFactory)
{
    m_action->setChildFactory(childFactory);
    m_action->setFlags(m_action->flags() | RequiresChildren);
    return *this;
}

} // namespace menu
} // namespace nx::vms::client::desktop
