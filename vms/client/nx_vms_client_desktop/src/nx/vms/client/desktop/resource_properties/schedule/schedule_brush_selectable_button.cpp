// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "schedule_brush_selectable_button.h"

#include <nx/vms/client/desktop/resource_properties/schedule/schedule_cell_painter.h>
#include <QtCore/QEvent>

namespace nx::vms::client::desktop {

ScheduleBrushSelectableButton::ScheduleBrushSelectableButton(QWidget* parent):
    base_type(parent)
{
}

ScheduleBrushSelectableButton::~ScheduleBrushSelectableButton()
{
}

QVariant ScheduleBrushSelectableButton::buttonBrush() const
{
    return m_buttonBrush;
}

void ScheduleBrushSelectableButton::setButtonBrush(const QVariant& brushData)
{
    m_buttonBrush = brushData;
    update();
}

ScheduleCellPainter* ScheduleBrushSelectableButton::cellPainter() const
{
    return m_cellPainter;
}

void ScheduleBrushSelectableButton::setCellPainter(ScheduleCellPainter* cellPainter)
{
    if (m_cellPainter == cellPainter)
        return;

    if (m_cellPainter)
        setCustomPaintFunction({});

    m_cellPainter = cellPainter;

    if (m_cellPainter)
    {
        setCustomPaintFunction(
            [this](QPainter* painter, const QStyleOption* option, const QWidget*)
            {
                const auto hovered = option->state.testFlag(QStyle::State_MouseOver);
                m_cellPainter->paintCellBackground(painter, option->rect, hovered, m_buttonBrush);
                return true;
            });
    }
    update();
}

} // namespace nx::vms::client::desktop
