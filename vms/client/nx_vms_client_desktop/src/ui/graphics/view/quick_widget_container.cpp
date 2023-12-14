// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "quick_widget_container.h"

namespace nx::vms::client::desktop {

void QuickWidgetContainer::setQuickWidget(QQuickWidget* quickWidget)
{
    m_quickWidget = quickWidget;
    m_quickWidget->setParent(this);
    m_quickWidget->setGeometry(0, 0, width(), height());
}

QQuickWidget* QuickWidgetContainer::quickWidget() const
{
    return m_quickWidget;
}

void QuickWidgetContainer::resizeEvent(QResizeEvent* event)
{
    if (m_quickWidget)
        m_quickWidget->setGeometry(QRect(QPoint(0, 0), event->size()));
}

void QuickWidgetContainer::moveEvent(QMoveEvent*)
{
    if (m_quickWidget)
        m_quickWidget->setGeometry(QRect(QPoint(0, 0), size()));
}

void QuickWidgetContainer::showEvent(QShowEvent*)
{
    if (m_quickWidget)
        m_quickWidget->setGeometry(QRect(QPoint(0, 0), size()));
}

} // namespace nx::vms::client::desktop
