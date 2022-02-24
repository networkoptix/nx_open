// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <QtWidgets/QAction>

#include <client/client_globals.h>
#include <common/common_globals.h>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_fwd.h>
#include <nx/vms/client/desktop/ui/actions/action_types.h>

namespace nx::vms::client::desktop {
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
    Action* action() const;

    Builder shortcut(const QString& keySequence);
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
    Builder requiredGlobalPermission(GlobalPermission permission);
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
} // namespace nx::vms::client::desktop
