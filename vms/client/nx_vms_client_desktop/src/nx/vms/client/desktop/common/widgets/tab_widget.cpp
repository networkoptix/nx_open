// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tab_widget.h"

namespace nx::vms::client::desktop {

TabWidget::TabWidget(QWidget* parent):
    TabWidget(nullptr, parent)
{
}

TabWidget::TabWidget(QTabBar* tabBar, QWidget* parent):
    base_type(parent)
{
    if (tabBar)
        setTabBar(tabBar);
}

} // namespace nx::vms::client::desktop
