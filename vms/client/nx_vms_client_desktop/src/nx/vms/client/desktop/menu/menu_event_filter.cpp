// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "menu_event_filter.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolTip>

#include "action.h"
#include "action_manager.h"

namespace nx::vms::client::desktop {
namespace menu {

MenuEventFilter::MenuEventFilter(QObject* parent): base_type{parent}
{
}

bool MenuEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        const auto helpEvent = static_cast<QHelpEvent*>(event);
        const auto menu = qobject_cast<QMenu*>(watched);
        const auto action = menu->actionAt(helpEvent->pos());
        if (action && !action->isEnabled())
        {
            if (const auto sourceAction = Manager::sourceAction(action))
            {
                const QString toolTip = sourceAction->disabledToolTip();
                if (!toolTip.isEmpty())
                    QToolTip::showText(helpEvent->globalPos(), toolTip, menu);
                else
                    QToolTip::hideText();
            }
        }
        else
        {
            QToolTip::hideText();
        }
    }
    return base_type::eventFilter(watched, event);
}

} // namespace menu
} // namespace nx::vms::client::desktop
