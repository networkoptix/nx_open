#include "software_trigger_button.h"

#include <ui/common/geometry.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/slider_tooltip_widget.h>
#include <ui/processors/hover_processor.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

static constexpr int kHoverEnterDelayMs = 0;
static constexpr int kHoverLeaveDelayMs = 250;
static constexpr qreal kToolTipAnimationSpeedFactor = 2.0;
static constexpr qreal kToolTipRoundingRadius = 2.0;
static constexpr qreal kToolTipPadding = 8.0;

static Qt::Edge invertEdge(Qt::Edge edge)
{
    switch (edge)
    {
        case Qt::LeftEdge:
            return Qt::RightEdge;

        case Qt::RightEdge:
            return Qt::LeftEdge;

        case Qt::TopEdge:
            return Qt::BottomEdge;

        case Qt::BottomEdge:
            return Qt::TopEdge;

        default:
            NX_ASSERT(false);
            return Qt::LeftEdge;
    }
}

} // namespace

QnSoftwareTriggerButton::QnSoftwareTriggerButton(QGraphicsItem* parent):
    base_type(parent),
    m_toolTip(new QnSliderTooltipWidget(this)),
    m_toolTipHoverProcessor(new HoverFocusProcessor(this)),
    m_toolTipEdge(Qt::LeftEdge)
{
    connect(this, &QnSoftwareTriggerButton::geometryChanged,
        this, &QnSoftwareTriggerButton::updateToolTipPosition);

    m_toolTipHoverProcessor->addTargetItem(this);
    m_toolTipHoverProcessor->addTargetItem(m_toolTip);
    m_toolTipHoverProcessor->setHoverEnterDelay(kHoverEnterDelayMs);
    m_toolTipHoverProcessor->setHoverLeaveDelay(kHoverLeaveDelayMs);
    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverEntered,
        this, &QnSoftwareTriggerButton::updateToolTipVisibility);
    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverLeft,
        this, &QnSoftwareTriggerButton::updateToolTipVisibility);

    m_toolTip->setOpacity(0.0);
    m_toolTip->setRoundingRadius(kToolTipRoundingRadius);
    m_toolTip->setFlag(QGraphicsItem::ItemIgnoresTransformations, false);

    updateToolTipTailEdge();
}

QnSoftwareTriggerButton::~QnSoftwareTriggerButton()
{
}

QString QnSoftwareTriggerButton::toolTip() const
{
    return m_toolTip->text();
}

void QnSoftwareTriggerButton::setToolTip(const QString& toolTip)
{
    m_toolTip->setText(toolTip);
}

Qt::Edge QnSoftwareTriggerButton::toolTipEdge() const
{
    return m_toolTipEdge;
}

void QnSoftwareTriggerButton::setToolTipEdge(Qt::Edge edge)
{
    if (m_toolTipEdge == edge)
        return;

    m_toolTipEdge = edge;
    updateToolTipTailEdge();
}

void QnSoftwareTriggerButton::updateToolTipVisibility()
{
    static constexpr qreal kTransparent = 0.0;
    static constexpr qreal kOpaque = 1.0;

    const qreal targetOpacity =
        m_toolTipHoverProcessor->isHovered() && !m_toolTip->text().isEmpty()
            ? kOpaque
            : kTransparent;

    opacityAnimator(m_toolTip, kToolTipAnimationSpeedFactor)->animateTo(targetOpacity);
}

void QnSoftwareTriggerButton::updateToolTipTailEdge()
{
    m_toolTip->setTailEdge(invertEdge(m_toolTipEdge));
    updateToolTipPosition();
}

void QnSoftwareTriggerButton::updateToolTipPosition()
{
    const auto size = this->size();
    switch (m_toolTipEdge)
    {
        case Qt::LeftEdge:
            m_toolTip->pointTo({ -kToolTipPadding, size.height() / 2 });
            break;

        case Qt::RightEdge:
            m_toolTip->pointTo({ size.width() + kToolTipPadding - 1, size.height() / 2 });
            break;

        case Qt::TopEdge:
            m_toolTip->pointTo({ size.width() / 2, -kToolTipPadding });
            break;

        case Qt::BottomEdge:
            m_toolTip->pointTo({ size.width() / 2, size.height() + kToolTipPadding - 1 });
            break;

        default:
            NX_ASSERT(false);
    }
}

void QnSoftwareTriggerButton::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
    QnScopedPainterBrushRollback brushRollback(painter, palette().window());

    painter->drawEllipse(QnGeometry::eroded(rect().toAlignedRect(), 1));

    base_type::paint(painter, option, widget);
}
