// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_widget_hider.h"

#include <algorithm>
#include <limits>

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QWidget>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

LayoutWidgetHider::LayoutWidgetHider(
    QGridLayout* gridLayout,
    QList<QWidget*> widgets,
    Qt::AlignmentFlag alignment)
    :
    m_gridLayout(gridLayout),
    m_alignment(alignment)
{
    if (!NX_ASSERT(alignment == Qt::AlignLeft || alignment == Qt::AlignRight))
        return;

    for (QWidget* widget: widgets)
    {
        const int index = m_gridLayout->indexOf(widget);
        if (!NX_ASSERT(index != -1))
            return;

        int row, column, rowSpan, columnSpan;
        m_gridLayout->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
        if (m_row == -1)
            m_row = row;

        if (!NX_ASSERT(rowSpan == 1 && columnSpan == 1, "Spanning cells are not allowed"))
            return;
        if (!NX_ASSERT(row == m_row, "Widgets must be on the same row"))
            return;
        if (!NX_ASSERT(!m_widgets.contains(column), "Every widget column must be unique"))
            return;

        m_widgets[column] = widget;
    }
    const int columnRange = m_widgets.lastKey() - m_widgets.firstKey() + 1;
    if (!NX_ASSERT(columnRange == m_widgets.size(), "No gaps between cells allowed"))
        return;
    m_valid = true;
}

void LayoutWidgetHider::setVisible(QWidget* widget, bool visible)
{
    const bool contains = std::any_of(
        m_widgets.cbegin(), m_widgets.cend(),
        [widget](const QWidget* mapWidget) { return widget == mapWidget; });
    if (!NX_ASSERT(contains))
        return;
    if (widget->isVisible() == visible)
        return;

    widget->setVisible(visible);
    if (!m_valid) //< Provided minimal expected behaviour (QWidget::setVisible()) if not valid.
        return;

    // Count visible and remove all widgets from layout
    int visibleCount = 0;
    for (QWidget* mapWidget: m_widgets)
    {
        visibleCount += (int) mapWidget->isVisible();
        m_gridLayout->removeWidget(mapWidget);
    }

    // Add visible widgets to layout at successive columns aligned to left or right.
    int column = (m_alignment == Qt::AlignLeft)
        ? m_widgets.firstKey()
        : (m_widgets.lastKey() - visibleCount + 1);
    for (QWidget* mapWidget: m_widgets)
    {
        if (mapWidget->isVisible())
            m_gridLayout->addWidget(mapWidget, m_row, column++);
    }
}

void LayoutWidgetHider::hide(QWidget* widget)
{
    setVisible(widget, false);
}

void LayoutWidgetHider::show(QWidget* widget)
{
    setVisible(widget, true);
}

} // namespace nx::vms::client::desktop
