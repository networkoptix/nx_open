// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "close_tab_button.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>

namespace nx::vms::client::desktop {

CloseTabButton::CloseTabButton(QWidget* parent): QAbstractButton(parent)
{
    setProperty(nx::style::Properties::kIsCloseTabButton, true);
    setFocusPolicy(Qt::NoFocus);
    QIcon icon = qnSkin->icon(
        "tab_bar/system_tab_close.svg",
        "tab_bar/system_tab_close_checked.svg",
        nullptr,
        Style::kTitleBarSubstitutions);
    setIcon(icon);
    setContentsMargins(0, 0, 4, 0);
}

void CloseTabButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    QRect rect = this->rect();
    QnIcon::Mode mode;
    QIcon::State state = QIcon::Off;

    if (!isEnabled())
        mode = QnIcon::Disabled;
    else if (isDown())
        mode = QnIcon::Pressed;
    else if (underMouse())
        mode = QnIcon::Active;
    else
        mode = QnIcon::Normal;

    if (const auto tb = qobject_cast<const QTabBar*>(parent()))
    {
        int index = tb->currentIndex();
        auto position = static_cast<QTabBar::ButtonPosition>(
            style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, tb));
        if (tb->tabButton(index, position) == this)
            state = QIcon::On;

        auto margins = contentsMargins();
        rect.moveLeft(position == QTabBar::RightSide ? -margins.right() : margins.left());
        rect.moveTop(margins.top());
    }
    icon().paint(&p, rect, Qt::AlignCenter, mode, state);
}

QSize CloseTabButton::sizeHint() const
{
    QSize result = qnSkin->maximumSize(icon());
    auto margins = contentsMargins();
    result += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return result;
}

} // namespace nx::vms::client::desttop
