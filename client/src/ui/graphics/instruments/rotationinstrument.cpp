#include "rotationinstrument.h"
#include <cassert>
#include <cmath> /* For std::fmod. */
#include <limits>
#include <QMouseEvent>
#include <QGraphicsObject>
#include <ui/graphics/items/resource_widget.h>

namespace {
    const QColor defaultRotationItemColor(255, 0, 0, 128);

    const qreal defaultRotationItemPenWidth = 2;

    const qreal defaultRotationHeadLength = 50;

    const QSizeF defaultRotationArrowSize = QSizeF(3, 5); /* (Side, Front) */

    inline void paintArrowHead(QPainter *painter, const QPointF &arrowTip, const QPointF &frontDelta, const QPointF &sideDelta) {
        QPointF points[3] = {
            arrowTip - frontDelta - sideDelta,
            arrowTip,
            arrowTip - frontDelta + sideDelta
        };
        
        painter->drawPolyline(points, 3);
    }

    QPointF mapFromRelative(QnResourceWidget *widget, const QPointF &point) {
        QSizeF size = widget->size();

        return QPointF(
            size.width()  * point.x(),
            size.height() * point.y()
        );
    }

} // anonymous namespace

class RotationItem: public QGraphicsObject, protected QnSceneUtility {
public:
    RotationItem(QGraphicsItem *parent = NULL): 
        QGraphicsObject(parent),
        m_viewport(NULL)
    {
        /* We cheat with the bounding rect, but properly calculating it is not worth it. */
        qreal d = std::numeric_limits<qreal>::max() / 4;
        m_boundingRect = QRectF(QPointF(-d, -d), QPoint(d, d));
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

        QPointF sceneOrigin = target()->mapToScene(mapFromRelative(target(), m_origin));

        /* Accessing viewport is safe here as it equals the passed widget. */
        QGraphicsView *view = QnSceneUtility::view(m_viewport);
        QTransform sceneToViewport = view->viewportTransform();

        /* Map head & origin to viewport. */
        QPointF viewportHead = sceneToViewport.map(m_sceneHead);
        QPointF viewportOrigin = sceneToViewport.map(sceneOrigin);

        /* Calculate "hammer" head delta. */
        QPointF unit = normalized(normal(viewportHead - viewportOrigin));
        QPointF headDelta = unit * defaultRotationHeadLength / 2;

        /* Calculate arrowhead deltas. */
        QPointF arrowFrontDelta = unit * defaultRotationArrowSize.height();
        QPointF arrowSideDelta = normal(unit) * defaultRotationArrowSize.width() / 2;

        /* Paint it all. */
        painter->save();
        painter->resetTransform();
        painter->setPen(QPen(defaultRotationItemColor, defaultRotationItemPenWidth));

        painter->drawLine(viewportHead, viewportOrigin);
        painter->drawLine(viewportHead - headDelta, viewportHead + headDelta);
        paintArrowHead(painter, viewportHead - headDelta, -arrowFrontDelta, arrowSideDelta);
        paintArrowHead(painter, viewportHead + headDelta,  arrowFrontDelta, arrowSideDelta);

        painter->restore();
    }

    /**
     * This item will be drawn only on the given viewport. 
     * This item won't access the given viewport in any way, so it is
     * safe to delete the viewport without notifying the item.
     * 
     * \param viewport                  Viewport to draw this item on.
     */
    void start(QWidget *viewport, QnResourceWidget *target, const QPointF &origin) {
        m_viewport = viewport;
        m_target = target;
        m_origin = origin;

        prepareGeometryChange();
    }

    void stop() {
        m_viewport = NULL;
        m_target.clear();

        prepareGeometryChange();
    }

    void setHead(const QPointF &head) {
        m_sceneHead = head;
    }

    const QPointF &origin() const {
        return m_origin;
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

    QnResourceWidget *target() const {
        return m_target.data();
    }

private:
    /** Viewport that this rotation item will be drawn at. */
    QWidget *m_viewport;

    /** Widget being rotated. */
    QWeakPointer<QnResourceWidget> m_target;

    /** Rotation origin point, in size-relative widget coordinates. */
    QPointF m_origin;

    /** Head of the rotation item, in scene coordinates. */
    QPointF m_sceneHead;

    /** Bounding rect of this item. */
    QRectF m_boundingRect;
};


RotationInstrument::RotationInstrument(QObject *parent):
    DragProcessingInstrument(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent)
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
    if(rotationItem() != NULL)
        delete rotationItem();

    m_rotationItem = new RotationItem();
    rotationItem()->setParent(this); /* Just to feel totally safe. */
    rotationItem()->setZValue(m_rotationItemZValue);
    rotationItem()->setVisible(false);
    scene()->addItem(rotationItem());

    DragProcessingInstrument::installedNotify();
}

void RotationInstrument::aboutToBeUninstalledNotify() {
    DragProcessingInstrument::aboutToBeUninstalledNotify();

    if (scene() != NULL) 
        disconnect(scene(), NULL, this, NULL);

    if(rotationItem() != NULL)
        delete rotationItem();
}

bool RotationInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    if(!(event->modifiers() & Qt::AltModifier))
        return false;

    QGraphicsView *view = this->view(viewport);
    QnResourceWidget *target = this->item<QnResourceWidget>(view, event->pos());
    if(target == NULL)
        return false;

    m_target = target;
    m_origin = QPointF(0.5, 0.5);
    m_originAngle = atan2(target->mapFromScene(view->mapToScene(event->pos())) - mapFromRelative(target, m_origin));
    
    dragProcessor()->mousePressEvent(viewport, event);
    
    event->accept();
    return false;
}

bool RotationInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return false;
    }

    return DragProcessingInstrument::paintEvent(viewport, event);
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

    rotationItem()->start(info->view()->viewport(), target(), m_origin);
    rotationItem()->setVisible(true);

    emit rotationStarted(info->view(), target());
    m_rotationStartedEmitted = true;
}

void RotationInstrument::dragMove(DragInfo *info) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return;
    }

    rotationItem()->setHead(info->mouseScenePos());

    qreal currentAngle = atan2(target()->mapFromScene(info->mouseScenePos()) - mapFromRelative(target(), m_origin));
    qreal currentRotation = target()->rotation();

    /* Graphics item rotation is clockwise, calculated angles are counter-clockwise, hence the "-". */
    qreal newRotation = currentRotation - (m_originAngle - currentAngle) / M_PI * 180.0;

    /* Clamp to [-180.0, 180.0]. */
    newRotation = std::fmod(newRotation + 180.0, 360.0) - 180.0;
    if(qFuzzyCompare(currentRotation, newRotation))
        return;

    target()->setRotation(newRotation);
}

void RotationInstrument::finishDrag(DragInfo *info) {
    if(m_rotationStartedEmitted)
        emit rotationFinished(info->view(), target());

    rotationItem()->setVisible(false);
}

void RotationInstrument::finishDragProcess(DragInfo *info) {
    emit rotationProcessFinished(info->view(), target());
}

