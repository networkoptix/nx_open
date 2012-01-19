#include "masked_proxy_widget.h"
#include <QStyleOptionGraphicsItem>
#include <ui/common/scene_utility.h>

QnMaskedProxyWidget::QnMaskedProxyWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    QGraphicsProxyWidget(parent, windowFlags)
{}

QnMaskedProxyWidget::~QnMaskedProxyWidget() {
    return;
}

void QnMaskedProxyWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(widget);

    if (!this->widget() || !this->widget()->isVisible())
        return;

    /* Filter out repaints on the window frame. */
    QRectF renderRectF = option->exposedRect & rect();

    /* Filter out areas that are masked out. */
    if(!m_paintRect.isNull())
        renderRectF = renderRectF & m_paintRect;

    /* Nothing to paint? Just return. */
    QRect renderRect = renderRectF.toAlignedRect();
    if (renderRect.isEmpty())
        return;

    /* Disable QPainter's default pen being cosmetic. This allows widgets and
     * styles to follow Qt's existing defaults without getting ugly cosmetic
     * lines when scaled. */
    bool restore = !(painter->renderHints() & QPainter::NonCosmeticDefaultPen);
    painter->setRenderHints(QPainter::NonCosmeticDefaultPen, true);

    this->widget()->render(painter, renderRect.topLeft(), renderRect);

    /* Restore the render hints if necessary. */
    if (restore)
        painter->setRenderHints(QPainter::NonCosmeticDefaultPen, false);
}

void QnMaskedProxyWidget::setPaintRect(const QRectF &paintRect) {
    if(qFuzzyCompare(m_paintRect, paintRect))
        return;

    m_paintRect = paintRect;
    update();

    emit paintRectChanged();
}

QRectF QnMaskedProxyWidget::paintGeometry() const {
    if(m_paintRect.isNull())
        return QRectF();

    return QRectF(
        pos() + m_paintRect.topLeft(),
        m_paintRect.size()
    );
}

void QnMaskedProxyWidget::setPaintGeometry(const QRectF &paintGeometry) {
    setPaintRect(QRectF(
        paintGeometry.topLeft() - pos(),
        paintGeometry.size()
    ));
}

