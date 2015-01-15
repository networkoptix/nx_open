#include "grid_raised_cone_item.h"

#include <QtGui/QPainter>

#include <ui/animation/opacity_animator.h>
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
    m_widget(parent),
    m_rotation(0)
{
    connect(m_widget, SIGNAL(geometryChanged()), this, SLOT(updateGeometry()));
    connect(m_widget, SIGNAL(rotationChanged()), this, SLOT(updateGeometry()));
    connect(m_widget, SIGNAL(destroyed()),       this, SLOT(deleteLater()));
}

QnGridRaisedConeItem::~QnGridRaisedConeItem() {

}

QRectF QnGridRaisedConeItem::boundingRect() const {
    QRectF bounding = m_originRect.united(m_targetRect);
    if (qFuzzyIsNull(m_rotation))
        return bounding;

    QPointF c = bounding.center();

    QTransform transform;
    transform.translate(c.x(), c.y());
    transform.rotate(m_rotation);
    transform.translate(-c.x(), -c.y());

    return transform.mapRect(bounding);
}


void QnGridRaisedConeItem::setEffectEnabled(bool value) {
    if (m_effectEnabled == value)
        return;
    m_effectEnabled = value;
    updateGeometry();
}


void QnGridRaisedConeItem::setOriginGeometry(QRectF originGeometry) {
    m_originRect = originGeometry;
    updateGeometry();
}


void QnGridRaisedConeItem::updateGeometry() {
    prepareGeometryChange();

    m_targetRect = m_widget->geometry();
    m_rotation = m_widget->rotation();

    qreal coneOpacity = m_effectEnabled
            ? m_originRect.width() > 0
              ? qBound(0.0, m_targetRect.width() / m_originRect.width() - 1.0, 1.0)
              : 0.0
              : 0.0;
#if 0
    qreal widgetOpacity = m_effectEnabled
            ? 1.0 - (1.0 - qnGlobals->raisedWigdetOpacity())*coneOpacity
            : 1.0;
#endif

    opacityAnimator(this)->animateTo(coneOpacity);
#if 0
    opacityAnimator(m_widget)->animateTo(widgetOpacity);
#endif
}


void QnGridRaisedConeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    PathHelper sourceHelper;
    sourceHelper.offset = qMin(m_originRect.height(), m_originRect.width()) * 0.1;
    sourceHelper.center = m_originRect.center();

    PathHelper targetHelper;
    targetHelper.offset = qMin(m_targetRect.height(), m_targetRect.width()) * 0.1;
    targetHelper.center = m_targetRect.center();

    painter->fillPath(createBeamPath(m_originRect.topLeft(),
                                     m_targetRect.topLeft(),
                                     sourceHelper,
                                     targetHelper,
                                     m_rotation,
                                     1, 1), qnGlobals->raisedConeColor());

    painter->fillPath(createBeamPath(m_originRect.topRight(),
                                     m_targetRect.topRight(),
                                     sourceHelper,
                                     targetHelper,
                                     m_rotation,
                                     -1, 1), qnGlobals->raisedConeColor());

    painter->fillPath(createBeamPath(m_originRect.bottomLeft(),
                                     m_targetRect.bottomLeft(),
                                     sourceHelper,
                                     targetHelper,
                                     m_rotation,
                                     1, -1), qnGlobals->raisedConeColor());

    painter->fillPath(createBeamPath(m_originRect.bottomRight(),
                                     m_targetRect.bottomRight(),
                                     sourceHelper,
                                     targetHelper,
                                     m_rotation,
                                     -1, -1), qnGlobals->raisedConeColor());

    // translate the whole painter is much easier than translate all 8 points of the path --gdm
    QPointF offset = m_originRect.center();
    painter->translate(offset);
    painter->rotate(m_rotation);
    painter->translate(-offset);

    painter->fillPath(createSourceRectPath(m_originRect, sourceHelper.offset), qnGlobals->raisedConeColor());

    painter->translate(offset);
    painter->rotate(-m_rotation);
    painter->translate(-offset);

}

