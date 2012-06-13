#include "rotation_instrument.h"

#include <cassert>
#include <cmath> /* For std::fmod, std::floor, std::sin and std::cos. */
#include <limits>

#include <QtGui/QMouseEvent>
#include <QtGui/QGraphicsWidget>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/checked_cast.h>

namespace {
    const QColor defaultColor(255, 0, 0, 96);

    const qreal defaultPenWidth = 10;

    const qreal defaultHeadLength = 120;

    const qreal defaultMinRadius = defaultHeadLength / M_PI;

    const qreal defaultIgnoredRadius = defaultPenWidth;

    const QSizeF defaultArrowSize = QSizeF(20, 35); /* (Side, Front) */
    
    const qreal defaultArrowOverlap = 5;

    const qreal snapTo90IntervalSize = 5.0;

    inline void addArrowHead(QPainterPath *shape, const QPointF &base, const QPointF &frontUnit, const QPointF &sideUnit) {
        shape->lineTo(base - defaultArrowSize.width() / 2 * sideUnit - defaultArrowOverlap * frontUnit);
        shape->lineTo(base + defaultArrowSize.height() * frontUnit);
        shape->lineTo(base + defaultArrowSize.width() / 2 * sideUnit - defaultArrowOverlap * frontUnit);
    }

    inline QPointF polar(qreal alpha, qreal r) {
        return QPointF(r * std::cos(alpha), r * std::sin(alpha));
    }

    const qreal centroidBorder = 0.5;

    QPointF calculateOrigin(QGraphicsView *view, QGraphicsWidget *widget) {
        QRect viewportRect = view->viewport()->rect();
        qreal viewportDiameter = QnGeometry::length(view->mapToScene(viewportRect.bottomRight()) - view->mapToScene(viewportRect.topLeft()));

        QRectF widgetRect = widget->rect();
        qreal widgetDiameter = QnGeometry::length(widget->mapToScene(widgetRect.bottomRight()) - widget->mapToScene(widgetRect.topLeft()));

        QPointF widgetCenter = widget->mapToScene(widget->transformOriginPoint());

        qreal k = viewportDiameter / widgetDiameter;
        if(k >= 1.0) {
            return widgetCenter;
        } else {
            QPointF viewportCenter = view->mapToScene(viewportRect.center());

            /* Perform calculations in the dimension of the line connecting viewport and widget centers,
             * zero at viewport center. */
            qreal distance = QnGeometry::length(viewportCenter - widgetCenter);
            qreal lo = qMax(-viewportDiameter / 2, distance - widgetDiameter / 2);
            qreal hi = qMin(viewportDiameter / 2, distance + widgetDiameter / 2);
            qreal pos = (lo + hi) / 2;

            /* Go back to 2D. */
            QPointF center = ((distance - pos) * viewportCenter + pos * widgetCenter) / distance;

            if(k <= centroidBorder) {
                return center;
            } else {
                qreal alpha = (k - centroidBorder) / (1.0 - centroidBorder);

                return alpha * widgetCenter + (1.0 - alpha) * center;
            }
        }
    }

    qreal calculateItemAngle(QGraphicsWidget *, const QPointF &itemPoint, const QPointF &itemOrigin) {
        return QnGeometry::atan2(itemPoint - itemOrigin);
    }

    qreal calculateSceneAngle(QGraphicsWidget *widget, const QPointF &scenePoint, const QPointF &sceneOrigin) {
        return calculateItemAngle(widget, widget->mapFromScene(scenePoint), widget->mapFromScene(sceneOrigin));
    }


} // anonymous namespace

class RotationItem: public QGraphicsObject, protected QnGeometry {
public:
    RotationItem(QGraphicsItem *parent = NULL): 
        QGraphicsObject(parent),
        m_viewport(NULL)
    {
        /* We cheat with the bounding rect, but properly calculating it is not worth it. */
        qreal d = std::numeric_limits<qreal>::max() / 4;
        m_boundingRect = QRectF(QPointF(-d, -d), QPointF(d, d));
    }

    virtual QRectF boundingRect() const override {
        if (m_viewport == NULL)
            return QRectF();

        return m_boundingRect;
    }

    /* We can also implement shape(), but most probably there is no point. */

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) override {
        if (widget != m_viewport)
            return; /* Draw it on source viewport only. */

        if(target() == NULL)
            return; /* Target may get suddenly deleted. */

        /* Accessing viewport is safe here as it equals the passed widget. */
        QGraphicsView *view = checked_cast<QGraphicsView *>(m_viewport->parent());
        QTransform sceneToViewport = view->viewportTransform();

        /* Map head & origin to viewport. */
        QPointF viewportHead = sceneToViewport.map(m_sceneHead);
        QPointF viewportOrigin = sceneToViewport.map(m_sceneOrigin);

        /* Precalculate shape parameters. */
        qreal radius = qMax(defaultMinRadius, this->length(viewportOrigin - viewportHead));

        qreal width = defaultPenWidth;
        qreal halfWidth = defaultPenWidth / 2;

        qreal headAngle = (defaultHeadLength / 2.0) / radius;
        qreal lineAngle = halfWidth / radius;
        qreal headAngleDegrees = headAngle / M_PI * 180.0;
        qreal lineAngleDegrees = lineAngle / M_PI * 180.0;

        /* Prepare shape. */
        QPainterPath shape;
        shape.moveTo(0, halfWidth);
        shape.arcTo(QRectF(-halfWidth, -halfWidth, width, width), -90.0, -180.0);
        shape.arcTo(QRectF(-radius + halfWidth, -radius + halfWidth, 2 * radius - width, 2 * radius - width), lineAngleDegrees, headAngleDegrees - lineAngleDegrees);
        addArrowHead(&shape, polar(-headAngle, radius), polar(-headAngle - M_PI / 2, 1), polar(-headAngle, 1));
        shape.arcTo(QRectF(-radius - halfWidth, -radius - halfWidth, 2 * radius + width, 2 * radius + width), headAngleDegrees, -2.0 * headAngleDegrees);
        addArrowHead(&shape, polar(headAngle, radius), polar(headAngle + M_PI / 2, 1), -polar(headAngle, 1));
        shape.arcTo(QRectF(-radius + halfWidth, -radius + halfWidth, 2 * radius - width, 2 * radius - width), -headAngleDegrees, headAngleDegrees - lineAngleDegrees);
        shape.closeSubpath();

        /* Draw! */
        QnScopedPainterTransformRollback transformRollback(painter, QTransform());
        painter->translate(viewportOrigin);
        painter->rotate(atan2(viewportHead - viewportOrigin) / M_PI * 180.0);
        painter->fillPath(shape, defaultColor);
    }

    /**
     * This item will be drawn only on the given viewport. 
     * This item won't access the given viewport in any way, so it is
     * safe to delete the viewport without notifying the item.
     * 
     * \param viewport                  Viewport to draw this item on.
     */
    void start(QWidget *viewport, QGraphicsWidget *target) {
        m_viewport = viewport;
        m_target = target;

        prepareGeometryChange();
    }

    void stop() {
        m_viewport = NULL;
        m_target.clear();

        prepareGeometryChange();
    }

    void setOrigin(const QPointF &origin) {
        m_sceneOrigin = origin;
    }

    void setHead(const QPointF &head) {
        m_sceneHead = head;
    }

    const QPointF &origin() const {
        return m_sceneOrigin;
    }

    const QPointF &head() const {
        return m_sceneHead;
    }

    /**
     * \returns                         This item's viewport.
     */
    QWidget *viewport() const {
        return m_viewport;
    }

    QGraphicsWidget *target() const {
        return m_target.data();
    }

private:
    /** Viewport that this rotation item will be drawn at. */
    QWidget *m_viewport;

    /** Widget being rotated. */
    QWeakPointer<QGraphicsWidget> m_target;

    /** Head of the rotation item, in scene coordinates. */
    QPointF m_sceneHead;

    /** Origin of the rotation item, in scene coordinates. */
    QPointF m_sceneOrigin;

    /** Bounding rect of this item. */
    QRectF m_boundingRect;
};


RotationInstrument::RotationInstrument(QObject *parent):
    base_type(Viewport, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent)
{
    /* We want to get drags on all paint events. */
    dragProcessor()->setFlags(DragProcessor::DONT_COMPRESS);
}

RotationInstrument::~RotationInstrument() {
    ensureUninstalled();
}

void RotationInstrument::setRotationItemZValue(qreal rotationItemZValue) {
    m_rotationItemZValue = rotationItemZValue;

    if(rotationItem() != NULL)
        rotationItem()->setZValue(m_rotationItemZValue);
}

void RotationInstrument::installedNotify() {
    assert(rotationItem() == NULL);

    m_rotationItem = new RotationItem();
    rotationItem()->setParent(this); /* Just to feel totally safe. */
    rotationItem()->setZValue(m_rotationItemZValue);
    rotationItem()->setVisible(false);
    scene()->addItem(rotationItem());

    base_type::installedNotify();
}

void RotationInstrument::aboutToBeUninstalledNotify() {
    base_type::aboutToBeUninstalledNotify();

    if(rotationItem() != NULL)
        delete rotationItem();
}

bool RotationInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    if(!(event->modifiers() & Qt::AltModifier))
        return false;

    QGraphicsView *view = this->view(viewport);
    QGraphicsWidget *target = this->item<QGraphicsWidget>(view, event->pos());
    if(!target || !satisfiesItemConditions(target))
        return false;

    m_target = target;
    m_originAngle = calculateSceneAngle(target, view->mapToScene(event->pos()), calculateOrigin(view, target));
    
    dragProcessor()->mousePressEvent(viewport, event);
    
    event->accept();
    return false;
}

bool RotationInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return false;
    }

    return base_type::paintEvent(viewport, event);
}

void RotationInstrument::startDragProcess(DragInfo *info) {
    emit rotationProcessStarted(info->view(), target());
}

void RotationInstrument::startDrag(DragInfo *info) {
    m_rotationStartedEmitted = false;

    if(target() == NULL) {
        /* Whoops, already destroyed. */
        dragProcessor()->reset();
        return;
    }

    rotationItem()->start(info->view()->viewport(), target());
    rotationItem()->setVisible(true);
    m_lastRotation = target()->rotation();

    emit rotationStarted(info->view(), target());
    m_rotationStartedEmitted = true;
}

void RotationInstrument::dragMove(DragInfo *info) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return;
    }

    /* Make sure that rotation didn't change since the last call. 
     * We may get some nasty effects if we don't do this. */
    target()->setRotation(m_lastRotation);

    QPointF sceneOrigin = calculateOrigin(info->view(), target());
    QPoint viewportOrigin = info->view()->mapFromScene(sceneOrigin);
    if(length(viewportOrigin - info->mouseViewportPos()) < defaultIgnoredRadius)
        return;

    QPointF itemOrigin = target()->mapFromScene(sceneOrigin);

    qreal currentAngle = calculateItemAngle(target(), target()->mapFromScene(info->mouseScenePos()), itemOrigin);
    qreal currentRotation = target()->rotation();

    /* Graphics item rotation is clockwise, calculated angles are counter-clockwise, hence the "-". */
    qreal newRotation = currentRotation - (m_originAngle - currentAngle) / M_PI * 180.0;

    /* Clamp to [-180.0, 180.0]. */
    newRotation = std::fmod(newRotation + 180.0, 360.0) - 180.0;

    /* Always snap to 90 degrees. */
    qreal closest90 = std::floor(newRotation / 90.0 + 0.5) * 90.0;
    if(std::abs(newRotation - closest90) < snapTo90IntervalSize / 2)
        newRotation = closest90;

    /* Round to 15 degrees if ctrl is pressed. */
    if(info->modifiers() & Qt::ControlModifier)
        newRotation = std::floor(newRotation / 15.0 + 0.5) * 15.0;

    /* Rotate item if needed. */
    if(!qFuzzyCompare(currentRotation, newRotation)) {
        target()->setRotation(newRotation);

        if(!qFuzzyCompare(target()->transformOriginPoint(), itemOrigin)) {
            QPointF newSceneOrigin = target()->mapToScene(itemOrigin);
            moveViewportScene(info->view(), newSceneOrigin - sceneOrigin);
            sceneOrigin = newSceneOrigin;
        }
    }

    /* Calculate rotation head position in scene coordinates. */
    qreal sceneAngle = m_originAngle + newRotation / 180.0 * M_PI;
    qreal headDistance = length(sceneOrigin - info->mouseScenePos());
    QPointF sceneHead = sceneOrigin + polar(sceneAngle, headDistance);

    /* Update rotation item. */
    rotationItem()->setHead(sceneHead);
    rotationItem()->setOrigin(sceneOrigin);
    
    m_lastRotation = target()->rotation();
}

void RotationInstrument::finishDrag(DragInfo *info) {
    if(m_rotationStartedEmitted)
        emit rotationFinished(info->view(), target());

    rotationItem()->setVisible(false);
}

void RotationInstrument::finishDragProcess(DragInfo *info) {
    emit rotationProcessFinished(info->view(), target());
}

