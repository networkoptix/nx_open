#include "notification_tooltip_widget.h"

QnNotificationToolTipWidget::QnNotificationToolTipWidget(QGraphicsItem* parent):
    base_type(parent)
{
}

void QnNotificationToolTipWidget::updateTailPos()
{
    if (m_enclosingRect.isNull())
        return;

    if (!parentItem())
        return;

    if (!parentItem()->parentItem())
        return;

    QRectF rect = this->rect();
    QGraphicsItem* list = parentItem()->parentItem();

    const auto parentY = parentItem()->mapToItem(list, m_pointTo).y();
    const auto tailX = qRound(rect.right() + tailLength());

    const auto toolTipHeight = rect.height();
    const auto halfHeight = toolTipHeight / 2;

    const auto spaceToTop = parentY - m_enclosingRect.top();
    const auto spaceToBottom = m_enclosingRect.bottom() - parentY;

    static const int kOffset = tailWidth() / 2;

    // Check if we are too close to the top (or there is not enough space in any case).
    if (spaceToTop < halfHeight || m_enclosingRect.height() < toolTipHeight)
    {
        const auto tailY = qRound(rect.top() + spaceToTop - kOffset);
        setTailPos(QPointF(tailX, tailY));
    }
    // Check if we are too close to the bottom.
    else if (spaceToBottom < halfHeight)
    {
        const auto tailY = qRound(rect.bottom() - spaceToBottom + kOffset);
        setTailPos(QPointF(tailX, tailY));
    }
    // Optimal position.
    else
    {
        const auto tailY = qRound((rect.top() + rect.bottom()) / 2);
        setTailPos(QPointF(tailX, tailY));
    }

    // Cannot call base_type as it is reimplemented.
    QnToolTipWidget::pointTo(m_pointTo);
}

void QnNotificationToolTipWidget::setEnclosingGeometry(const QRectF& enclosingGeometry)
{
    m_enclosingRect = enclosingGeometry;
    updateTailPos();
}
