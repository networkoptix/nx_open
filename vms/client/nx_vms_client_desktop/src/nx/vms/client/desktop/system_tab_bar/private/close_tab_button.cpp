// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "close_tab_button.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTabBar>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>

#include "../home_tab_button.h"
#include "../system_tab_bar.h"

namespace {

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kTabCloseTheme = {
    {QnIcon::Normal, {.primary = "dark16"}},
    {QnIcon::Active, {.primary = "light10"}},
    {QnIcon::Pressed, {.primary = "dark13"}},
};

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kTabCloseCheckedTheme = {
    {QnIcon::Normal, {.primary = "light4"}},
    {QnIcon::Active, {.primary = "light1"}},
    {QnIcon::Pressed, {.primary = "light7"}},
};

NX_DECLARE_COLORIZED_ICON(kSystemTabBar,
    "16x16/Outline/tab_close.svg",
    kTabCloseTheme,
    "16x16/Outline/tab_close.svg",
    kTabCloseCheckedTheme)

};  // namespace

namespace nx::vms::client::desktop {

CloseTabButton::CloseTabButton(QWidget* parent): QAbstractButton(parent)
{
    setProperty(nx::style::Properties::kIsCloseTabButton, true);
    setFocusPolicy(Qt::NoFocus);
    QIcon icon = qnSkin->icon(kSystemTabBar);
    setIcon(icon);
    setFixedSize(kFixedIconWidth, kFixedIconHeight);
}

void CloseTabButton::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
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

    if (const auto tabWidget = qobject_cast<const QTabBar*>(parent()))
    {
        const int currentIndex = tabWidget->currentIndex();
        const auto position = static_cast<QTabBar::ButtonPosition>(
            style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, tabWidget));
        if (tabWidget->tabButton(currentIndex, position) == this)
        {
            const auto systemTabBar = qobject_cast<const SystemTabBar*>(tabWidget);
            if (!systemTabBar || systemTabBar->active())
                state = QIcon::On;
        }
    }
    else if (const auto buttonWidget = qobject_cast<const HomeTabButton*>(parent()))
    {
        state = QIcon::On;
    }
    icon().paint(&painter, rect, Qt::AlignLeft | Qt::AlignVCenter, mode, state);
}

CloseTabButton* CloseTabButton::createForTab(QTabBar* tabBar, int index)
{
    if (!NX_ASSERT(tabBar))
        return nullptr;

    auto button = new CloseTabButton(tabBar);
    tabBar->setTabButton(index, QTabBar::RightSide, button);
    QObject::connect(button, &CloseTabButton::clicked, tabBar,
        [button, tabBar]() { emit tabBar->tabCloseRequested(tabBar->tabAt(button->pos())); });

    return button;
}

} // namespace nx::vms::client::desttop
