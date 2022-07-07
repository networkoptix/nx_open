// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "close_tab_button.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

namespace nx::vms::client::desktop {

CloseTabButton::CloseTabButton(QWidget* parent): QAbstractButton(parent)
{
    setFocusPolicy(Qt::NoFocus);
    int width = style()->pixelMetric(QStyle::PM_TabCloseIndicatorWidth, 0, this);
    int height = style()->pixelMetric(QStyle::PM_TabCloseIndicatorHeight, 0, this);
    setFixedSize(width, height);
}

void CloseTabButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    QStyleOption opt;
    opt.initFrom(this);
    opt.state |= QStyle::State_AutoRaise;
    if (isEnabled() && underMouse() && !isChecked() && !isDown())
        opt.state |= QStyle::State_Raised;
    if (isChecked())
        opt.state |= QStyle::State_On;
    if (isDown())
        opt.state |= QStyle::State_Sunken;

    if (const auto tb = qobject_cast<const QTabBar*>(parent()))
    {
        int index = tb->currentIndex();
        auto position = static_cast<QTabBar::ButtonPosition>(
            style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, tb));
        if (tb->tabButton(index, position) == this)
            opt.state |= QStyle::State_Selected;
    }
    style()->drawPrimitive(QStyle::PE_IndicatorTabClose, &opt, &p, this);
}

} // namespace nx::vms::client::desttop
