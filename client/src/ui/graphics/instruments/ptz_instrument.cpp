#include "ptz_instrument.h"

#include <QtGui/QGraphicsSceneMouseEvent>

#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>

#include <core/resource/camera_resource.h>
#include <core/resource/video_server_resource.h>
#include <core/resourcemanagment/resource_pool.h>

#include <api/video_server_connection.h>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/resource/media_resource_widget.h>


namespace {
    // TODO: merge with rotation instrument

    const QColor arrowColor(255, 0, 0, 96);

    const qreal arrowWidth = 10;

    const QSizeF headSize = QSizeF(25, 35); /* (Side, Front) */

    const qreal headOverlap = 5;

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

class PtzItem: public QGraphicsObject, protected QnGeometry {
public:
    PtzItem(QGraphicsItem *parent = NULL): 
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
        qreal scale = 1.0 + m_magnitude * 3.0;
        qreal width = arrowWidth;
        qreal halfWidth = arrowWidth / 2;
        qreal headDistance = qMax(width, this->length(viewportOrigin - viewportHead) / scale - headSize.height());
        qreal baseDistance = headDistance - qMin(headDistance / 3, headSize.height() * 2);

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
        painter->translate(viewportOrigin);
        painter->scale(scale, scale);
        painter->rotate(atan2(viewportHead - viewportOrigin) / M_PI * 180.0);
        painter->fillPath(shape, arrowColor);
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

    void setTarget(QnMediaResourceWidget *target) {
        m_target = target;
    }

    void setOrigin(const QPointF &origin) {
        m_sceneOrigin = origin;
    }

    void setHead(const QPointF &head) {
        m_sceneHead = head;
    }

    void setMagnitude(qreal magnitude) {
        m_magnitude = magnitude;
    }

    const QPointF &origin() const {
        return m_sceneOrigin;
    }

    const QPointF &head() const {
        return m_sceneHead;
    }

    qreal magnitude() const {
        return m_magnitude;
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

    /** Widget being manipulated. */
    QWeakPointer<QGraphicsWidget> m_target;

    /** Head of the ptz item, in scene coordinates. */
    QPointF m_sceneHead;

    /** Origin of the ptz item, in scene coordinates. */
    QPointF m_sceneOrigin;

    /** Scale of the ptz item. */
    qreal m_magnitude;

    /** Bounding rect of this item. */
    QRectF m_boundingRect;
};

PtzInstrument::PtzInstrument(QObject *parent): 
    base_type(
        makeSet(QEvent::HoverEnter, QEvent::HoverMove, QEvent::HoverLeave),
        makeSet(),
        makeSet(),
        makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), 
        parent
    ) 
{}

PtzInstrument::~PtzInstrument() {
    ensureUninstalled();
}

void PtzInstrument::setPtzItemZValue(qreal ptzItemZValue) {
    m_ptzItemZValue = ptzItemZValue;

    if(ptzItem())
        ptzItem()->setZValue(m_ptzItemZValue);
}

void PtzInstrument::installedNotify() {
    assert(ptzItem() == NULL);

    m_ptzItem = new PtzItem();
    ptzItem()->setParent(this); /* Just to feel totally safe. Note that this is a parent in QObject sense. */
    ptzItem()->setZValue(m_ptzItemZValue);
    ptzItem()->setVisible(false);
    ptzItem()->setOpacity(ptzItemBackgroundOpacity);
    scene()->addItem(ptzItem());

    base_type::installedNotify();
}

void PtzInstrument::aboutToBeUninstalledNotify() {
    base_type::aboutToBeUninstalledNotify();

    if(ptzItem() != NULL)
        delete ptzItem();
}

bool PtzInstrument::registeredNotify(QGraphicsItem *item) {
    return base_type::registeredNotify(item) && dynamic_cast<QnMediaResourceWidget *>(item);
}

bool PtzInstrument::event(QWidget *viewport, QEvent *event) {
    qDebug() << event;

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
            m_target = target;
            m_camera = camera;
            m_serverSpeed = QVector3D();
            m_localSpeed = QVector3D();
            m_connection = server->apiConnection();
            
            ptzItem()->setViewport(event->widget());

            dragProcessor()->mousePressEvent(item, event);
        }
    }

    event->accept();
    return false;
}

void PtzInstrument::startDragProcess(DragInfo *) {
    emit ptzProcessStarted(target());

    opacityAnimator(ptzItem(), ptzItemOpacityAnimationSpeed)->animateTo(ptzItemOperationalOpacity);
}

void PtzInstrument::startDrag(DragInfo *info) {
    emit ptzStarted(target());

    ptzItem()->setTarget(target());
    ptzItem()->setVisible(true);
}

void PtzInstrument::dragMove(DragInfo *info) {
    QPointF delta = info->mouseItemPos() - target()->rect().center();
    QSizeF size = target()->size();
    qreal side = qMax(size.width(), size.height());

    QPointF speed = cwiseDiv(delta, QSizeF(side, side) / 2.0);
    m_localSpeed.setX(qBound(-1.0, speed.x(), 1.0));
    m_localSpeed.setY(qBound(-1.0, speed.y(), 1.0));

    if((m_localSpeed - m_serverSpeed).lengthSquared() > instantUpdateSpeedThreshold * instantUpdateSpeedThreshold) {
        m_connection->asyncPtzMove(m_camera, m_localSpeed.x(), -m_localSpeed.y(), m_localSpeed.z(), this, SLOT(at_replyReceived(int, int)));
        m_serverSpeed = m_localSpeed;

        qDebug() << "PTZ set speed " << m_localSpeed;
    }

    /* Calculate head & origin positions in scene coordinates. */
    QPointF sceneHead = info->mouseScenePos();
    QPointF sceneOrigin = target()->mapToScene(target()->rect().center());

    /* Update rotation item. */
    ptzItem()->setHead(sceneHead);
    ptzItem()->setOrigin(sceneOrigin);
    ptzItem()->setMagnitude(qMax(qAbs(m_localSpeed.x()), qAbs(m_localSpeed.y())));
}

void PtzInstrument::finishDrag(DragInfo *) {
    emit ptzFinished(target());

    ptzItem()->setVisible(false);
}

void PtzInstrument::finishDragProcess(DragInfo *) {
    if(!m_serverSpeed.isNull()) {
        m_connection->asyncPtzStop(m_camera, this, SLOT(at_replyReceived(int, int)));
        m_serverSpeed = QVector3D();
    }

    m_localSpeed = QVector3D();
    m_camera = QnNetworkResourcePtr();
    m_connection = QnVideoServerConnectionPtr();

    opacityAnimator(ptzItem(), ptzItemOpacityAnimationSpeed)->animateTo(ptzItemBackgroundOpacity);

    emit ptzProcessFinished(target());
}

void PtzInstrument::at_replyReceived(int status, int handle) {
    Q_UNUSED(status);
    Q_UNUSED(handle);
}




