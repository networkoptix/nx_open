#include "grid_raised_cone_item.h"

#include <QPainter>

namespace {

    /**
     * @brief createBeamPath    Creates painter path for graphics beam from source corner to target corner.
     * @param source            Source corner point.
     * @param target            Target corner point.
     * @param offsetSource      Absolute offset value that will be used on source rect.
     * @param offsetTarget      Absolute offset value that will be used on target rect.
     * @param signX             Correct sign of x-coord offset.
     * @param signY             Correct sign of y-coord offset.
     * @return                  Painter path.
     */
    QPainterPath createBeamPath(QPointF source,
                                QPointF target,
                                qreal offsetSource,
                                qreal offsetTarget,
                                int signX,
                                int signY) {

        QPainterPath path;

        path.setFillRule(Qt::WindingFill);

        path.moveTo(target);
        path.lineTo(source);
        path.lineTo(source);
        path.lineTo(source.x() + signX * offsetSource, source.y());
        path.lineTo(target.x() + signX * offsetTarget, target.y());
        path.closeSubpath();

        path.moveTo(target);
        path.lineTo(source);
        path.lineTo(source);
        path.lineTo(source.x(), source.y() + signY * offsetSource);
        path.lineTo(target.x(), target.y() + signY * offsetTarget);
        path.closeSubpath();
        return path;
    }

    QPainterPath createSourceRectPath(QRectF source, qreal offset) {
        QPainterPath path;
        path.addRect(source);
        path.addRect(source.left() + offset, source.top() + offset, source.width()- offset*2, source.height() - offset*2);
        return path;
    }

}

QnGridRaisedConeItem::QnGridRaisedConeItem(QGraphicsItem *parent) :
    QGraphicsObject(parent),
    m_raisedWidget(NULL)
{
}

QnGridRaisedConeItem::~QnGridRaisedConeItem() {

}

QRectF QnGridRaisedConeItem::boundingRect() const {
    return m_sourceRect.united(m_targetRect);
}


QGraphicsWidget* QnGridRaisedConeItem::raisedWidget() const {
    return m_raisedWidget;
}

void QnGridRaisedConeItem::setRaisedWidget(QGraphicsWidget *widget, QRectF oldGeometry) {
    if (m_raisedWidget != NULL)
        disconnect(m_raisedWidget, 0, this, 0);

    m_raisedWidget = widget;
    setVisible(m_raisedWidget != NULL);
    if(m_raisedWidget == NULL)
        return;

    connect(m_raisedWidget, SIGNAL(geometryChanged()), this, SLOT(updateGeometry()));
    m_sourceRect = oldGeometry;
    updateGeometry();
}


void QnGridRaisedConeItem::updateGeometry() {
    if(m_raisedWidget == NULL)
        return;
    prepareGeometryChange();
    m_targetRect = m_raisedWidget->geometry();
}

void QnGridRaisedConeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)
    if(m_raisedWidget == NULL)
        return;

    const qreal offsetSource = qMin(m_sourceRect.height(), m_sourceRect.width()) * 0.2;
    const qreal offsetTarget = qMin(m_targetRect.height(), m_targetRect.width()) * 0.1;;

    painter->fillPath(createBeamPath(m_sourceRect.topLeft(),
                                     m_targetRect.topLeft(),
                                     offsetSource,
                                     offsetTarget,
                                     1, 1), Qt::green);

    painter->fillPath(createBeamPath(m_sourceRect.topRight(),
                                     m_targetRect.topRight(),
                                     offsetSource,
                                     offsetTarget,
                                     -1, 1), Qt::green);

    painter->fillPath(createBeamPath(m_sourceRect.bottomLeft(),
                                     m_targetRect.bottomLeft(),
                                     offsetSource,
                                     offsetTarget,
                                     1, -1), Qt::green);

    painter->fillPath(createBeamPath(m_sourceRect.bottomRight(),
                                     m_targetRect.bottomRight(),
                                     offsetSource,
                                     offsetTarget,
                                     -1, -1), Qt::green);

    painter->fillPath(createSourceRectPath(m_sourceRect, offsetSource), Qt::green);


}

