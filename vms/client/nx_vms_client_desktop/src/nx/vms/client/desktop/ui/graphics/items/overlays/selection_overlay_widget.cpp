// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "selection_overlay_widget.h"

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsScene>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <nx/vms/client/desktop/style/style.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

namespace {

static constexpr qreal kSelectionOpacity = 0.2;

bool hasFrameDistinctionColor(QnResourceWidget* widget)
{
    return widget->frameDistinctionColor().isValid();
}

static QnResourceWidget::SelectionState unfocusedState(QnResourceWidget::SelectionState base)
{
    switch (base)
    {
        case QnResourceWidget::SelectionState::selected:
            return QnResourceWidget::SelectionState::notSelected;

        case QnResourceWidget::SelectionState::focusedAndSelected:
        case QnResourceWidget::SelectionState::focused:
            return QnResourceWidget::SelectionState::inactiveFocused;

        default:
            return base;
    }
}

} // namespace

namespace nx::vms::client::desktop {
namespace ui {

SelectionWidget::SelectionWidget(QnResourceWidget* parent):
    GraphicsWidget(parent),
    m_widget(parent)
{
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
    setFocusPolicy(Qt::NoFocus);
}

void SelectionWidget::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    paintSelection(painter);

    if (!qFuzzyIsNull(m_widget->frameOpacity()))
    {
        const QnWorkbenchLayout* layout = m_widget->item()->layout();
        if (!layout) //< Paint called while item is being removed.
            return;

        if (qFuzzyIsNull(layout->cellSpacing()))
            paintInnerFrame(painter);
        else
            paintOuterFrame(painter);
    }
}

QColor SelectionWidget::calculateFrameColor() const
{
    auto state = m_widget->selectionState();
    if (!m_widget->hasFocus() && !(m_widget->scene() && m_widget->scene()->hasFocus()))
        state = unfocusedState(state);

    switch (state)
    {
        case QnResourceWidget::SelectionState::focusedAndSelected:
        case QnResourceWidget::SelectionState::selected:
            return core::colorTheme()->color("brand_l");

        case QnResourceWidget::SelectionState::focused:
            return hasFrameDistinctionColor(m_widget)
                ? m_widget->frameDistinctionColor().lighter()
                : core::colorTheme()->color("brand");

        case QnResourceWidget::SelectionState::inactiveFocused:
            return hasFrameDistinctionColor(m_widget)
                ? m_widget->frameDistinctionColor()
                : core::colorTheme()->color("dark10");

        default:
            return hasFrameDistinctionColor(m_widget)
                ? m_widget->frameDistinctionColor()
                : Qt::transparent;
    }
}

void SelectionWidget::paintSelection(QPainter* painter)
{
    if (!m_widget->isSelected())
        return;

    if (!m_widget->options().testFlag(QnResourceWidget::DisplaySelection))
        return;

    const PainterTransformScaleStripper scaleStripper(painter);
    painter->fillRect(scaleStripper.mapRect(rect()),
        toTransparent(core::colorTheme()->color("brand"),
        kSelectionOpacity));
}

void SelectionWidget::paintInnerFrame(QPainter* painter)
{
    // Skip inner frame painting in some states. Zoom windows must always have frame.
    if (!hasFrameDistinctionColor(m_widget))
    {
        switch (m_widget->selectionState())
        {
            case QnResourceWidget::SelectionState::invalid:
            case QnResourceWidget::SelectionState::notSelected:
            case QnResourceWidget::SelectionState::selected:
                return;
            default:
                break;
        }
    }

    static const int kInnerBorderWidth = 2;
    const auto effectiveOpacity = painter->opacity() * m_widget->frameOpacity();
    QnScopedPainterOpacityRollback opacityRollback(painter, effectiveOpacity);
    Style::paintCosmeticFrame(painter, rect(), calculateFrameColor(),
        kInnerBorderWidth, 0);
}

void SelectionWidget::paintOuterFrame(QPainter* painter)
{
    // Skip outer frame painting in some states. Zoom windows must always have frame.
    if (!hasFrameDistinctionColor(m_widget))
    {
        switch (m_widget->selectionState())
        {
            case QnResourceWidget::SelectionState::invalid:
            case QnResourceWidget::SelectionState::notSelected:
                return;
            default:
                break;
        }
    }

    int mainBorderWidth = 1;

    // Increase border width for focused widgets.
    switch (m_widget->selectionState())
    {
        case QnResourceWidget::SelectionState::inactiveFocused:
        case QnResourceWidget::SelectionState::focused:
        case QnResourceWidget::SelectionState::focusedAndSelected:
            mainBorderWidth = 2;
            break;
        default:
            break;
    }

    const auto effectiveOpacity = painter->opacity() * m_widget->frameOpacity();
    QnScopedPainterOpacityRollback opacityRollback(painter, effectiveOpacity);

    // Dark outer border right near the camera.
    static const int kSpacerBorderWidth = 1;
    const QColor spacerBorderColor = core::colorTheme()->color("dark4");
    Style::paintCosmeticFrame(painter, rect(), spacerBorderColor,
        kSpacerBorderWidth, -kSpacerBorderWidth);

    Style::paintCosmeticFrame(painter, rect(), calculateFrameColor(),
        mainBorderWidth, -kSpacerBorderWidth - mainBorderWidth);
}

} // namespace ui
} // namespace nx::vms::client::desktop
