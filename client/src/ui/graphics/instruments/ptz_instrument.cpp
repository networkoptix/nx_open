#include "ptz_instrument.h"

#include <limits>

#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QVector2D>

#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>

#include <core/resource/camera_resource.h>
#include <core/resource/video_server_resource.h>
#include <core/resourcemanagment/resource_pool.h>

#include <api/video_server_connection.h>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include "ui/animation/animated.h"

namespace {
    // TODO: merge with rotation instrument

    const QColor arrowColor(255, 0, 0, 112);

    const QColor arrowOutlineColor(0, 0, 0, 128);

    const qreal arrowWidth = 5;

    const QSizeF headSize = QSizeF(12.5, 17.5); /* (Side, Front) */

    const qreal headOverlap = 2.5;

    const qreal maximalScale = 6.0;

    inline void addArrowHead(QPainterPath *shape, const QPointF &base, const QPointF &frontUnit, const QPointF &sideUnit) {
        shape->lineTo(base - headSize.width() / 2 * sideUnit - headOverlap * frontUnit);
        shape->lineTo(base + headSize.height() * frontUnit);
        shape->lineTo(base + headSize.width() / 2 * sideUnit - headOverlap * frontUnit);
    }


    const qreal instantUpdateSpeedThreshold = 0.1;
    const qreal delayedUpdateSpeedThreshold = 0.01;

    const qreal ptzItemBackgroundOpacity = 0.3;
    const qreal ptzItemOperationalOpacity = 1.0;
    const qreal ptzItemOpacityAnimationSpeed = 2.0;

} // anonymous namespace


// -------------------------------------------------------------------------- //
// PtzItem
// -------------------------------------------------------------------------- //
class PtzItem: public Animated<QGraphicsObject>, protected QnGeometry {
    typedef Animated<QGraphicsObject> base_type;

public:
    PtzItem(PtzInstrument *instrument, QGraphicsItem *parent = NULL): 
        base_type(parent),
        m_instrument(instrument),
        m_opacityAnimator(::opacityAnimator(this, ptzItemOpacityAnimationSpeed))
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
        if(!viewport() || !view() || widget != viewport())
            return; /* Draw it on source viewport only. */

        if(m_instrument->target() == NULL)
            return; /* Target may get suddenly deleted. */

        updateParameters();

        /* Precalculate shape parameters. */
        qreal scale = 1.0 + m_magnitude * (maximalScale - 1.0);
        qreal width = arrowWidth;
        qreal halfWidth = arrowWidth / 2;
        qreal headDistance = qMax(width, this->length(m_viewportOrigin - m_viewportHead) / scale - headSize.height());
        qreal baseDistance = headDistance - qMin(headDistance / 4, headSize.height());

        /* Prepare shape. */
        QPainterPath shape;
        shape.moveTo(baseDistance, halfWidth);
        shape.arcTo(QRectF(baseDistance - halfWidth, -halfWidth, width, width), -90.0, -180.0);
        shape.lineTo(headDistance, -halfWidth);
        addArrowHead(&shape, QPointF(headDistance, 0), QPointF(1, 0), QPointF(0, 1));
        shape.lineTo(headDistance, halfWidth);
        shape.closeSubpath();

        /* Draw! */
        QnScopedPainterTransformRollback transformRollback(painter, QTransform());
        QnScopedPainterPenRollback penRollback(painter, arrowOutlineColor);
        QnScopedPainterBrushRollback brushRollback(painter, arrowColor);
        painter->translate(m_viewportOrigin);
        painter->scale(scale, scale);
        painter->rotate(atan2(m_viewportHead - m_viewportOrigin) / M_PI * 180.0);
        painter->drawPath(shape);
    }

    /**
     * This item will be drawn only on the given view. 
     * 
     * \param viewport                  Viewport to draw this item on.
     */
    void setViewport(QWidget *viewport) {
        if(m_viewport.data() == viewport)
            return;

        m_viewport = viewport;
        m_view = viewport ? checked_cast<QGraphicsView *>(viewport->parent()) : NULL;
        prepareGeometryChange();

        updateParameters();
    }

    /**
     * \returns                         This item's viewport.
     */
    QWidget *viewport() const {
        return m_viewport.data();
    }

    /**
     * \returns                         This item's graphics view.
     */
    QGraphicsView *view() const {
        return m_view.data();
    }

    /**
     * \returns                         This item's opacity animator.
     */
    VariantAnimator *opacityAnimator() const {
        return m_opacityAnimator;
    }

    const QVector2D &speed() const {
        return m_speed;
    }

private:
    void updateParameters() {
        if(!viewport() || !m_instrument->target())
            return;

        m_viewportHead = viewport()->mapFromGlobal(QCursor::pos());
        
        QPointF head = m_instrument->target()->mapFromScene(view()->mapToScene(m_viewportHead));
        QPointF origin = m_instrument->target()->rect().center();
        QPointF delta = head - origin;

        m_viewportOrigin = view()->mapFromScene(m_instrument->target()->mapToScene(origin));

        QSizeF size = m_instrument->target()->size();
        qreal side = qMax(size.width(), size.height());
        QPointF speed = cwiseDiv(delta, QSizeF(side, side) / 2.0);
        m_speed = QVector2D(
            qBound(-1.0, speed.x(), 1.0),
            qBound(-1.0, speed.y(), 1.0)
        );

        m_magnitude = qMax(qAbs(m_speed.x()), qAbs(m_speed.y()));
    }

private:
    PtzInstrument *m_instrument;

    /** Viewport that this rotation item will be drawn at. */
    QWeakPointer<QWidget> m_viewport;

    /** View that this rotation item will be drawn at. */
    QWeakPointer<QGraphicsView> m_view;

    /** Bounding rect of this item. */
    QRectF m_boundingRect;

    /** Item's head, in viewport coordinates. */
    QPoint m_viewportHead;

    /** Item's origin, in viewport coordinates. */
    QPoint m_viewportOrigin;

    /** Speed. */
    QVector2D m_speed;

    /** Speed magnitude to be reflected in item's size. */
    qreal m_magnitude;

    /** Opacity animator of this item. */
    VariantAnimator *m_opacityAnimator;
};


// -------------------------------------------------------------------------- //
// PtzInstrument
// -------------------------------------------------------------------------- //
PtzInstrument::PtzInstrument(QObject *parent): 
    base_type(
        makeSet(QEvent::MouseMove),
        makeSet(),
        makeSet(),
        makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease, QEvent::GraphicsSceneHoverEnter, QEvent::GraphicsSceneHoverMove, QEvent::GraphicsSceneHoverLeave), 
        parent
    ),
    m_ptzItemZValue(0.0),
    m_targetUnderMouse(false)
{
    dragProcessor()->setStartDragTime(0);
}

PtzInstrument::~PtzInstrument() {
    ensureUninstalled();
}

void PtzInstrument::setPtzItemZValue(qreal ptzItemZValue) {
    m_ptzItemZValue = ptzItemZValue;

    if(ptzItem())
        ptzItem()->setZValue(m_ptzItemZValue);
}

void PtzInstrument::setTarget(QnMediaResourceWidget *target) {
    if(this->target() == target)
        return;

    m_target = target;
    ptzItem()->opacityAnimator()->stop();
    ptzItem()->setOpacity(0.0);

    setTargetUnderMouse(false);
}

void PtzInstrument::setTargetUnderMouse(bool underMouse) {
    if(m_targetUnderMouse == underMouse)
        return;
    
    m_targetUnderMouse = underMouse;

    updatePtzItemOpacity();
}

void PtzInstrument::updatePtzItemOpacity() {
    if(target() && isTargetUnderMouse() && (target()->options() & QnResourceWidget::ControlPtz)) {
        ptzItem()->opacityAnimator()->animateTo(dragProcessor()->isWaiting() ? ptzItemBackgroundOpacity : ptzItemOperationalOpacity);
    } else {
        ptzItem()->opacityAnimator()->animateTo(0.0);
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void PtzInstrument::installedNotify() {
    assert(ptzItem() == NULL);

    m_ptzItem = new PtzItem(this);
    ptzItem()->setParent(this); /* Just to feel totally safe. Note that this is a parent in QObject sense. */
    ptzItem()->setZValue(m_ptzItemZValue);
    ptzItem()->setOpacity(0.0);
    scene()->addItem(ptzItem());

    base_type::installedNotify();
}

void PtzInstrument::aboutToBeUninstalledNotify() {
    base_type::aboutToBeUninstalledNotify();

    if(ptzItem())
        delete ptzItem();
}

bool PtzInstrument::registeredNotify(QGraphicsItem *item) {
    bool result = base_type::registeredNotify(item);
    if(!result)
        return false;

    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item);
    if(widget) {
        connect(widget, SIGNAL(optionsChanged()), this, SLOT(at_target_optionsChanged()));
        return true;
    } else {
        return false;
    }
}

void PtzInstrument::unregisteredNotify(QGraphicsItem *item) {
    /* We don't want to use RTTI at this point, so we don't cast to QnMediaResourceWidget. */
    QGraphicsObject *object = item->toGraphicsObject();
    disconnect(object, NULL, this, NULL);
}

bool PtzInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *) {
    /* Make sure ptz item is displayed on the viewport where the mouse is. */
    ptzItem()->setViewport(viewport);

    return false;
}

bool PtzInstrument::hoverEnterEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) {
    QnMediaResourceWidget *target = checked_cast<QnMediaResourceWidget *>(item);
    setTarget(target);
    setTargetUnderMouse(true);

    return false;
}

bool PtzInstrument::hoverMoveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) {
    return hoverEnterEvent(item, event);
}

bool PtzInstrument::hoverLeaveEvent(QGraphicsItem *item, QGraphicsSceneHoverEvent *event) {
    setTargetUnderMouse(false);
    
    return false;
}

bool PtzInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    QnMediaResourceWidget *target = checked_cast<QnMediaResourceWidget *>(item);
    if(!(target->options() & QnResourceWidget::ControlPtz))
        return false;

    if(!target->rect().contains(event->pos()))
        return false; /* Ignore clicks on widget frame. */

    if(QnNetworkResourcePtr camera = target->resource().dynamicCast<QnNetworkResource>()) {
        if(QnVideoServerResourcePtr server = camera->resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnVideoServerResource>()) {
            m_serverSpeed = QVector3D();
            m_camera = camera;
            m_connection = server->apiConnection();
            
            dragProcessor()->mousePressEvent(item, event);
        }
    }

    event->accept();
    return false;
}

void PtzInstrument::startDragProcess(DragInfo *) {
    emit ptzProcessStarted(target());

    updatePtzItemOpacity();
}

void PtzInstrument::startDrag(DragInfo *info) {
    emit ptzStarted(target());
}

void PtzInstrument::dragMove(DragInfo *info) {
    QVector3D localSpeed = ptzItem()->speed();

    if((localSpeed - m_serverSpeed).lengthSquared() > instantUpdateSpeedThreshold * instantUpdateSpeedThreshold) {
        m_connection->asyncPtzMove(m_camera, localSpeed.x(), -localSpeed.y(), localSpeed.z(), this, SLOT(at_replyReceived(int, int)));
        m_serverSpeed = localSpeed;

        // TODO: timeout setter.

        qDebug() << "PTZ set speed " << localSpeed;
    }
}

void PtzInstrument::finishDrag(DragInfo *) {
    emit ptzFinished(target());
}

void PtzInstrument::finishDragProcess(DragInfo *) {
    if(!m_serverSpeed.isNull()) {
        m_connection->asyncPtzStop(m_camera, this, SLOT(at_replyReceived(int, int)));
        m_serverSpeed = QVector3D();
    }

    m_camera = QnNetworkResourcePtr();
    m_connection = QnVideoServerConnectionPtr();

    updatePtzItemOpacity();

    emit ptzProcessFinished(target());
}

void PtzInstrument::at_replyReceived(int status, int handle) {
    Q_UNUSED(status);
    Q_UNUSED(handle);
}

void PtzInstrument::at_target_optionsChanged() {
    if(sender() == target())
        updatePtzItemOpacity();
}


