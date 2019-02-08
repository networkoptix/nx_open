
#include "styled_tooltip_widget.h"

#include <limits>

#include <QtWidgets/QApplication>

#include <ui/common/palette.h>
#include <ui/style/helper.h>

#include <nx/utils/math/fuzzy.h>

namespace {

const qreal kDefaultRoundingRadius = 2.0;

} // namespace


QnStyledTooltipWidget::QnStyledTooltipWidget(QGraphicsItem* parent):
    base_type(parent),
    m_tailLength(style::Metrics::kStandardPadding),
    m_tailEdge(Qt::BottomEdge)
{
    setRoundingRadius(kDefaultRoundingRadius);
    setTailWidth(m_tailLength * 2.0);

    const int margin = style::Metrics::kStandardPadding;
    setContentsMargins(margin, margin, margin, margin);

    setZValue(std::numeric_limits<qreal>::max());
    updateTailPos();
}

QnStyledTooltipWidget::~QnStyledTooltipWidget()
{
}

qreal QnStyledTooltipWidget::tailLength() const
{
    return m_tailLength;
}

void QnStyledTooltipWidget::setTailLength(qreal tailLength)
{
    if (qFuzzyEquals(tailLength, m_tailLength))
        return;

    m_tailLength = tailLength;
    updateTailPos();
}

Qt::Edge QnStyledTooltipWidget::tailEdge() const
{
    return m_tailEdge;
}

void QnStyledTooltipWidget::setTailEdge(Qt::Edge tailEdge)
{
    if (m_tailEdge == tailEdge)
        return;

    const auto pointingTo = mapToParent(tailPos());
    m_tailEdge = tailEdge;
    updateTailPos();
    pointTo(pointingTo);
}

void QnStyledTooltipWidget::setGeometry(const QRectF& rect)
{
    QSizeF oldSize = size();

    base_type::setGeometry(rect);

    if(size() != oldSize)
        updateTailPos();
}

void QnStyledTooltipWidget::updateTailPos()
{
    QRectF rect = this->rect();
    switch (m_tailEdge)
    {
        case Qt::LeftEdge:
            setTailPos(QPointF(rect.left() - m_tailLength, (rect.top() + rect.bottom()) / 2));
            break;

        case Qt::RightEdge:
            setTailPos(QPointF(rect.right() + m_tailLength, (rect.top() + rect.bottom()) / 2));
            break;

        case Qt::TopEdge:
            setTailPos(QPointF((rect.left() + rect.right()) / 2, rect.top() - m_tailLength));
            break;

        case Qt::BottomEdge:
        default:
            setTailPos(QPointF((rect.left() + rect.right()) / 2, rect.bottom() + m_tailLength));
            break;
    }
}
