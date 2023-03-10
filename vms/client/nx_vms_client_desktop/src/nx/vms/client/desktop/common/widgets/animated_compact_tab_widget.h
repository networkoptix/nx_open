// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "animated_tab_widget.h"

namespace nx::vms::client::desktop {

class CompactTabBar;

class AnimatedCompactTabWidget: public AnimatedTabWidget
{
    Q_OBJECT

public:
    explicit AnimatedCompactTabWidget(QWidget* parent = nullptr);
    explicit AnimatedCompactTabWidget(CompactTabBar* tabBar, QWidget* parent = nullptr);

    int insertIconWidgetTab(int index, QWidget* widget, QWidget* iconWidget, const QString& label);

private:
    CompactTabBar* m_compactTabBar = nullptr;
};

} // namespace nx::vms::client::desktop
