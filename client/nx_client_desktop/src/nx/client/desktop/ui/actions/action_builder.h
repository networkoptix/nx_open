#pragma once

#include <QtCore/QString>

#include <QtWidgets/QAction>

#include <common/common_globals.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_fwd.h>
#include <nx/client/desktop/ui/actions/action_types.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class Builder
{
public:
    enum Platform
    {
        AllPlatforms = -1,
        Windows,
        Linux,
        Mac
    };

    Builder(Action* action);

    Builder shortcut(const QKeySequence& keySequence, Platform platform, bool replaceExisting);
    Builder shortcut(const QKeySequence& keySequence);
    Builder shortcutContext(Qt::ShortcutContext context);
    Builder icon(const QIcon& icon);
    Builder iconVisibleInMenu(bool visible);
    Builder role(QAction::MenuRole role);
    Builder autoRepeat(bool autoRepeat);
    Builder text(const QString& text);
    Builder dynamicText(const TextFactoryPtr& factory);
    Builder toggledText(const QString& text);
    Builder pulledText(const QString& text);
    Builder toolTip(const QString& toolTip);
    Builder toolTipFormat(const QString& toolTipFormat);
    Builder flags(ActionFlags flags);
    Builder mode(ClientModes mode);
    Builder requiredGlobalPermission(Qn::GlobalPermission permission);
    Builder requiredTargetPermissions(int key, Qn::Permissions permissions);
    Builder requiredTargetPermissions(Qn::Permissions permissions);
    Builder separator(bool isSeparator = true);
    Builder conditionalText(const QString& text, ConditionWrapper&& condition);
    Builder checkable(bool isCheckable = true);
    Builder checked(bool isChecked = true);
    Builder showCheckBoxInMenu(bool show);
    Builder accent(Qn::ButtonAccent value);
    Builder condition(ConditionWrapper&& condition);
    Builder childFactory(const FactoryPtr& childFactory);

private:
    Action* m_action;
};

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
