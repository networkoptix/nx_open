#include "grid_raised_cone_item.h"

#include <QPainter>

#include <ui/style/globals.h>

namespace {

    const char *raisedConeItemPropertyName = "_qn_raisedConeItem";

    struct PathHelper {
        /** Absolute offset value that will be used on the rect */
        qreal offset;

        /** Center of the rect */
        QPointF center;
    };

    /**
     * @brief rotated       Rotates the point around the center by degrees angle
     */
    QPointF rotated(const QPointF &point, const QPointF& center, qreal degrees) {
        if (qFuzzyIsNull(degrees))
            return point;

        QTransform transform;
        transform.translate(center.x(), center.y());
        transform.rotate(degrees);
        transform.translate(-center.x(), -center.y());

        return transform.map(point);
    }

    /**
     * @brief createBeamPath    Creates painter path for graphics beam from source corner to target corner.
     * @param source            Source corner point.
     * @param target            Target corner point.
     * @param sourceHelper      Helper for the source rect.
     * @param targetHelper      Helper for the target rect.
     * @param rotation          Rotation in degrees.
     * @param signX             Correct sign of x-coord offset.
     * @param signY             Correct sign of y-coord offset.
     * @return                  Painter path.
     */
    QPainterPath createBeamPath(QPointF source,
                                QPointF target,
                                PathHelper sourceHelper,
                                PathHelper targetHelper,
                                qreal rotation,
                                int signX,
                                int signY) {

        QPainterPath path;

        path.setFillRule(Qt::WindingFill);

        path.moveTo(rotated(target, targetHelper.center, rotation));
        path.lineTo(rotated(source, sourceHelper.center, rotation));
        path.lineTo(rotated(QPointF(source.x() + signX * sourceHelper.offset, source.y()), sourceHelper.center, rotation));
        path.lineTo(rotated(QPointF(target.x() + signX * targetHelper.offset, target.y()), targetHelper.center, rotation));
        path.closeSubpath();

        path.moveTo(rotated(target, targetHelper.center, rotation));
        path.lineTo(rotated(source, sourceHelper.center, rotation));
        path.lineTo(rotated(QPointF(source.x(), source.y() + signY * sourceHelper.offset), sourceHelper.center, rotation));
        path.lineTo(rotated(QPointF(target.x(), target.y() + signY * targetHelper.offset), targetHelper.center, rotation));
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

QnGridRaisedConeItem* raisedConeItem(QGraphicsWidget* widget) {
    if(widget == NULL)
        return NULL;

    QnGridRaisedConeItem *item = widget->property(raisedConeItemPropertyName).value<QnGridRaisedConeItem *>();
    if(item)
        return item;


    item = new QnGridRaisedConeItem(widget);
    widget->setProperty(raisedConeItemPropertyName, QVariant::fromValue<QnGridRaisedConeItem *>(item));

    return item;
}


QnGridRaisedConeItem::QnGridRaisedConeItem(QGraphicsWidget *parent) :
    QGraphicsObject(NULL),
    m_raisedWidget(parent),
    m_rotation(0)
{
    connect(m_raisedWidget, SIGNAL(geometryChanged()), this, SLOT(updateGeometry()));
    connect(m_raisedWidget, SIGNAL(rotationChanged()), this, SLOT(updateGeometry()));
    connect(m_raisedWidget, SIGNAL(destroyed()),       this, SLOT(deleteLater()));
}

QnGridRaisedConeItem::~QnGridRaisedConeItem() {

}

QRectF QnGridRaisedConeItem::boundingRect() const {
    QRectF bounding = m_sourceRect.united(m_targetRect);
    if (qFuzzyIsNull(m_rotation))
        return bounding;

    QPointF c = bounding.center();

    QTransform transform;
    transform.translate(c.x(), c.y());
    transform.rotate(m_rotation);
    transform.translate(-c.x(), -c.y());

    return transform.mapRect(bounding);
}


void QnGridRaisedConeItem::adjustGeometry(QRectF oldGeometry) {
    m_sourceRect = oldGeometry;
    updateGeometry();
}


void QnGridRaisedConeItem::updateGeometry() {
    prepareGeometryChange();

    m_targetRect = m_raisedWidget->geometry();
    m_rotation = m_raisedWidget->rotation();
}


void QnGridRaisedConeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    PathHelper sourceHelper;
    sourceHelper.offset = qMin(m_sourceRect.height(), m_sourceRect.width()) * 0.2;
    sourceHelper.center = m_sourceRect.center();

    PathHelper targetHelper;
    targetHelper.offset = qMin(m_targetRect.height(), m_targetRect.width()) * 0.1;
    targetHelper.center = m_targetRect.center();

    painter->fillPath(createBeamPath(m_sourceRect.topLeft(),
                                     m_targetRect.topLeft(),
                                     sourceHelper,
                                     targetHelper,
                                     m_rotation,
                                     1, 1), qnGlobals->raisedConeColor());

    painter->fillPath(createBeamPath(m_sourceRect.topRight(),
                                     m_targetRect.topRight(),
                                     sourceHelper,
                                     targetHelper,
                                     m_rotation,
                                     -1, 1), qnGlobals->raisedConeColor());

    painter->fillPath(createBeamPath(m_sourceRect.bottomLeft(),
                                     m_targetRect.bottomLeft(),
                                     sourceHelper,
                                     targetHelper,
                                     m_rotation,
                                     1, -1), qnGlobals->raisedConeColor());

    painter->fillPath(createBeamPath(m_sourceRect.bottomRight(),
                                     m_targetRect.bottomRight(),
                                     sourceHelper,
                                     targetHelper,
                                     m_rotation,
                                     -1, -1), qnGlobals->raisedConeColor());

    // translate the whole painter is much easier than translate all 8 points of the path --gdm
    QPointF offset = m_sourceRect.center();
    painter->translate(offset);
    painter->rotate(m_rotation);
    painter->translate(-offset);

    painter->fillPath(createSourceRectPath(m_sourceRect, sourceHelper.offset), qnGlobals->raisedConeColor());

    painter->translate(offset);
    painter->rotate(-m_rotation);
    painter->translate(-offset);

}

