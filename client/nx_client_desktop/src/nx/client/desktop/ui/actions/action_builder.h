#pragma once

#include <QtCore/QString>

#include <QtWidgets/QAction>

#include <common/common_globals.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_conditions.h>

class QnAction;
class QnActionFactory;
class QnActionTextFactory;
class QnResourceCriterion;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class Builder
{
public:
    enum ActionPlatform
    {
        AllPlatforms = -1,
        Windows,
        Linux,
        Mac
    };

    Builder(QnAction* action);

    Builder shortcut(const QKeySequence& keySequence, ActionPlatform platform, bool replaceExisting);
    Builder shortcut(const QKeySequence& keySequence);
    Builder shortcutContext(Qt::ShortcutContext context);
    Builder icon(const QIcon& icon);
    Builder iconVisibleInMenu(bool visible);
    Builder role(QAction::MenuRole role);
    Builder autoRepeat(bool autoRepeat);
    Builder text(const QString& text);
    Builder dynamicText(QnActionTextFactory* factory);
    Builder toggledText(const QString& text);
    Builder pulledText(const QString& text);
    Builder toolTip(const QString& toolTip);
    Builder toolTipFormat(const QString& toolTipFormat);
    Builder flags(Qn::ActionFlags flags);
    Builder mode(QnActionTypes::ClientModes mode);
    Builder requiredGlobalPermission(Qn::GlobalPermission permission);
    Builder requiredTargetPermissions(int key, Qn::Permissions permissions);
    Builder requiredTargetPermissions(Qn::Permissions permissions);
    Builder separator(bool isSeparator = true);
    Builder conditionalText(const QString &text, QnActionCondition* condition);
    Builder conditionalText(const QString &text, const QnResourceCriterion& criterion,
        Qn::MatchMode matchMode = Qn::All);
    Builder checkable(bool isCheckable = true);
    Builder checked(bool isChecked = true);
    Builder showCheckBoxInMenu(bool show);
    Builder accent(Qn::ButtonAccent value);
    Builder condition(const QnActionConditionPtr& condition);
    Builder condition(const QnResourceCriterion& criterion, Qn::MatchMode matchMode = Qn::All);
    Builder childFactory(QnActionFactory* childFactory);

private:
    QnAction* m_action;
};

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
