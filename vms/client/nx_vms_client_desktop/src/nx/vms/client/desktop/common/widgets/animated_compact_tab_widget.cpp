// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "animated_compact_tab_widget.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/common/widgets/compact_tab_bar.h>

namespace nx::vms::client::desktop {

AnimatedCompactTabWidget::AnimatedCompactTabWidget(QWidget* parent):
    AnimatedCompactTabWidget(nullptr, parent)
{
}

AnimatedCompactTabWidget::AnimatedCompactTabWidget(CompactTabBar* tabBar, QWidget* parent):
    AnimatedTabWidget(tabBar, parent)
{
    m_compactTabBar = tabBar;
}

int AnimatedCompactTabWidget::insertIconWidgetTab(
    int index,
    QWidget* widget,
    QWidget* iconWidget,
    const QString& label)
{
    int result = AnimatedTabWidget::insertTab(index, widget, label);

    if (NX_ASSERT(m_compactTabBar))
        m_compactTabBar->setIconWidget(index, iconWidget);

    return result;
}

} // namespace nx::vms::client::desktop
