
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
    m_tailLength(style::Metrics::kStandardPadding)
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

void QnStyledTooltipWidget::setGeometry(const QRectF& rect)
{
    QSizeF oldSize = size();

    base_type::setGeometry(rect);

    if(size() != oldSize)
        updateTailPos();
}

void QnStyledTooltipWidget::updateTailPos() {
    QRectF rect = this->rect();
    setTailPos(QPointF((rect.left() + rect.right()) / 2, rect.bottom() + m_tailLength));
}
