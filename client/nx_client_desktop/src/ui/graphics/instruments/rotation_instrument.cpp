#include "rotation_instrument.h"

#include <cassert>
#include <cmath> /* For std::fmod, std::floor. */
#include <limits>

#include <QtGui/QMouseEvent>
#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QApplication>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/checked_cast.h>
#include <utils/common/warnings.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/client/core/utils/geometry.h>

#include <utils/math/coordinate_transformations.h>

using nx::client::core::utils::Geometry;

namespace {
    const QColor arrowColor(255, 0, 0, 96);

    const QColor arrowOutlineColor(0, 0, 0, 128);

    const qreal arrowWidth = 10;

    const qreal sidesLength = 120;

    const qreal minRadius = sidesLength / M_PI;

    const qreal ignoreRadius = arrowWidth;

    const QSizeF headSize = QSizeF(20, 35); /* (Side, Front) */

    const qreal headOverlap = 5;

    const qreal snapTo90IntervalSize = 5.0;

    inline void addArrowHead(QPainterPath *shape, const QPointF &base, const QPointF &frontUnit, const QPointF &sideUnit) {
        shape->lineTo(base - headSize.width() / 2 * sideUnit - headOverlap * frontUnit);
        shape->lineTo(base + headSize.height() * frontUnit);
        shape->lineTo(base + headSize.width() / 2 * sideUnit - headOverlap * frontUnit);
    }

    const qreal centroidBorder = 0.15;

    QPointF calculateOrigin(QGraphicsView *view, QGraphicsWidget *widget) {
        QRect viewportRect = view->viewport()->rect();
        qreal viewportDiameter = Geometry::length(view->mapToScene(viewportRect.bottomRight()) - view->mapToScene(viewportRect.topLeft()));

        QRectF widgetRect = widget->rect();
        qreal widgetDiameter = Geometry::length(widget->mapToScene(widgetRect.bottomRight()) - widget->mapToScene(widgetRect.topLeft()));

        QPointF widgetCenter = widget->mapToScene(widget->transformOriginPoint());

        qreal k = viewportDiameter / widgetDiameter;
        if(k >= 1.0) {
            return widgetCenter;
        } else {
            QPointF viewportCenter = view->mapToScene(viewportRect.center());

            /* Perform calculations in the dimension of the line connecting viewport and widget centers,
             * zero at viewport center. */
            qreal distance = Geometry::length(viewportCenter - widgetCenter);
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
        return Geometry::atan2(itemPoint - itemOrigin);
    }

    qreal calculateSceneAngle(QGraphicsWidget *widget, const QPointF &scenePoint, const QPointF &sceneOrigin) {
        return calculateItemAngle(widget, widget->mapFromScene(scenePoint), widget->mapFromScene(sceneOrigin));
    }

    struct ItemAcceptsLeftMouseButton: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->acceptedMouseButtons() & Qt::LeftButton;
        }
    };

} // anonymous namespace

class RotationItem: public QGraphicsObject
{
public:
    RotationItem(QGraphicsItem *parent = NULL):
        QGraphicsObject(parent),
        m_viewport(NULL)
    {
        /* We cheat with the bounding rect, but properly calculating it is not worth it. */
        qreal d = std::numeric_limits<qreal>::max() / 4;
        m_boundingRect = QRectF(QPointF(-d, -d), QPointF(d, d));

        setAcceptedMouseButtons(0);
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
        qreal radius = qMax(minRadius, Geometry::length(viewportOrigin - viewportHead));

        qreal width = arrowWidth;
        qreal halfWidth = arrowWidth / 2;

        qreal sideAngle = (sidesLength / 2.0) / radius;
        qreal lineAngle = halfWidth / radius;
        qreal sideAngleDegrees = sideAngle / M_PI * 180.0;
        qreal lineAngleDegrees = lineAngle / M_PI * 180.0;

        /* Prepare shape. */
        QPainterPath shape;
        shape.moveTo(0, halfWidth);
        shape.arcTo(QRectF(-halfWidth, -halfWidth, width, width), -90.0, -180.0);
        shape.arcTo(QRectF(-radius + halfWidth, -radius + halfWidth, 2 * radius - width, 2 * radius - width), lineAngleDegrees, sideAngleDegrees - lineAngleDegrees);
        addArrowHead(&shape, polarToCartesian<QPointF>(radius, -sideAngle), polarToCartesian<QPointF>(1, -sideAngle - M_PI / 2), polarToCartesian<QPointF>(1, -sideAngle));
        shape.arcTo(QRectF(-radius - halfWidth, -radius - halfWidth, 2 * radius + width, 2 * radius + width), sideAngleDegrees, -2.0 * sideAngleDegrees);
        addArrowHead(&shape, polarToCartesian<QPointF>(radius, sideAngle), polarToCartesian<QPointF>(1, sideAngle + M_PI / 2), -polarToCartesian<QPointF>(1, sideAngle));
        shape.arcTo(QRectF(-radius + halfWidth, -radius + halfWidth, 2 * radius - width, 2 * radius - width), -sideAngleDegrees, sideAngleDegrees - lineAngleDegrees);
        shape.closeSubpath();

        /* Draw! */
        QnScopedPainterTransformRollback transformRollback(painter, QTransform());
        QnScopedPainterPenRollback penRollback(painter, arrowOutlineColor);
        QnScopedPainterBrushRollback brushRollback(painter, arrowColor);
        painter->translate(viewportOrigin);
        painter->rotate(Geometry::atan2(viewportHead - viewportOrigin) / M_PI * 180.0);
        painter->drawPath(shape);
    }

    /**
     * This item will be drawn only on the given viewport.
     * This item won't access the given viewport in any way, so it is
     * safe to delete the viewport without notifying the item.
     *
     * \param viewport                  Viewport to draw this item on.
     */
    void setViewport(QWidget *viewport) {
        if(m_viewport != viewport)
            prepareGeometryChange();

        m_viewport = viewport;
    }

    void setTarget(QGraphicsWidget *target) {
        m_target = target;
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
    QPointer<QGraphicsWidget> m_target;

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
    dragProcessor()->setFlags(DragProcessor::DontCompress);
}

RotationInstrument::~RotationInstrument() {
    ensureUninstalled();
}

void RotationInstrument::setRotationItemZValue(qreal rotationItemZValue) {
    m_rotationItemZValue = rotationItemZValue;

    if(rotationItem())
        rotationItem()->setZValue(m_rotationItemZValue);
}

void RotationInstrument::installedNotify() {
    NX_ASSERT(rotationItem() == NULL);

    m_rotationItem = new RotationItem();
    rotationItem()->setParent(this); /* Just to feel totally safe. Note that this is a parent in QObject sense. */
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

void RotationInstrument::start(QGraphicsWidget *target) {
    start(NULL, target);
}

void RotationInstrument::start(QGraphicsView *view, QGraphicsWidget *target) {
    if(!target) {
        qnNullWarning(target);
        return;
    }

    if(!isEnabled())
        return;

    if(!satisfiesItemConditions(target))
        return;

    if(!view) {
        QGraphicsScene *scene = target->scene();
        if(scene && !scene->views().empty())
            view = scene->views().front();
    }
    if(!view)
        return;

    QMouseEvent event(
        QEvent::MouseButtonPress,
        view->mapFromGlobal(QCursor::pos()),
        QCursor::pos(),
        Qt::LeftButton,
        qApp->mouseButtons() | Qt::LeftButton,
        0
    );

    startInternal(view, &event, target, true);
}

RotationItem *RotationInstrument::rotationItem() const {
    return m_rotationItem.data();
}

QGraphicsWidget *RotationInstrument::target() const {
    return m_target.data();
}


void RotationInstrument::startInternal(QGraphicsView *view, QMouseEvent *event, QGraphicsWidget *target, bool instantStart) {
    m_target = target;
    m_sceneOrigin = calculateOrigin(view, target);
    m_originAngle = calculateSceneAngle(target, view->mapToScene(event->pos()), calculateOrigin(view, target));

    dragProcessor()->mousePressEvent(view->viewport(), event, instantStart);
}

bool RotationInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    if(!(event->modifiers() & Qt::AltModifier))
        return false;

    QGraphicsView *view = this->view(viewport);
    QGraphicsWidget *target = this->item<QGraphicsWidget>(view, event->pos(), ItemAcceptsLeftMouseButton());
    if(!target || !satisfiesItemConditions(target))
        return false;

    startInternal(view, event, target, false);

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

    rotationItem()->setViewport(info->view()->viewport());
    rotationItem()->setTarget(target());
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

    QPoint viewportOrigin = info->view()->mapFromScene(m_sceneOrigin);
    if(Geometry::length(viewportOrigin - info->mouseViewportPos()) < ignoreRadius)
        return;

    QPointF itemOrigin = target()->mapFromScene(m_sceneOrigin);

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
    if (!qFuzzyEquals(currentRotation, newRotation))
    {
        QSizeF itemSize = target()->size();
        QPointF itemCenter = target()->transformOriginPoint();

        target()->setRotation(newRotation);

        QPointF newItemCenter = target()->transformOriginPoint();
        qreal scale = Geometry::scaleFactor(itemSize, target()->size(), Qt::KeepAspectRatio);
        QPointF newItemOrigin = newItemCenter + (itemOrigin - itemCenter) * scale;

        itemOrigin = newItemOrigin;

        if(!qFuzzyEquals(target()->transformOriginPoint(), itemOrigin)) {
            QPointF newSceneOrigin = target()->mapToScene(itemOrigin);
            moveViewportScene(info->view(), newSceneOrigin - m_sceneOrigin);
            scaleViewport(info->view(), scale, info->view()->mapFromScene(newSceneOrigin));
            m_sceneOrigin = newSceneOrigin;
        }
    }

    /* Calculate rotation head position in scene coordinates. */
    qreal sceneAngle = m_originAngle + newRotation / 180.0 * M_PI;
    qreal headDistance = Geometry::length(m_sceneOrigin - info->mouseScenePos());
    QPointF sceneHead = m_sceneOrigin + polarToCartesian<QPointF>(headDistance, sceneAngle);

    /* Update rotation item. */
    rotationItem()->setHead(sceneHead);
    rotationItem()->setOrigin(m_sceneOrigin);

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

