#include "selection_overlay_widget.h"

#include <QtGui/QPainter>

#include <nx/client/desktop/ui/common/painter_transform_scale_stripper.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/style/nx_style.h>
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

} // namespace

namespace nx {
namespace client {
namespace desktop {
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
        if (qFuzzyIsNull(m_widget->item()->layout()->cellSpacing()))
            paintInnerFrame(painter);
        else
            paintOuterFrame(painter);
    }
}

QColor SelectionWidget::calculateFrameColor() const
{
    switch (m_widget->selectionState())
    {
        case QnResourceWidget::SelectionState::focusedAndSelected:
        case QnResourceWidget::SelectionState::selected:
            return qnNxStyle->mainColor(QnNxStyle::Colors::kBrand);

        case QnResourceWidget::SelectionState::focused:
            return hasFrameDistinctionColor(m_widget)
                ? m_widget->frameDistinctionColor().lighter()
                : qnNxStyle->mainColor(QnNxStyle::Colors::kBrand).darker(4);

        case QnResourceWidget::SelectionState::inactiveFocused:
            return hasFrameDistinctionColor(m_widget)
                ? m_widget->frameDistinctionColor()
                : qnNxStyle->mainColor(QnNxStyle::Colors::kBase).lighter(3);

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
        toTransparent(qnNxStyle->mainColor(QnNxStyle::Colors::kBrand),
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
    QnNxStyle::paintCosmeticFrame(painter, rect(), calculateFrameColor(),
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
    const QColor spacerBorderColor = qnNxStyle->mainColor(QnNxStyle::Colors::kBase).darker(3);
    QnNxStyle::paintCosmeticFrame(painter, rect(), spacerBorderColor,
        kSpacerBorderWidth, -kSpacerBorderWidth);

    QnNxStyle::paintCosmeticFrame(painter, rect(), calculateFrameColor(),
        mainBorderWidth, -kSpacerBorderWidth - mainBorderWidth);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
