#include "rotationinstrument.h"
#include <cassert>
#include <cmath>
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

        assert(target() != NULL);

        QPointF sceneOrigin = target()->mapToScene(m_origin);

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
        m_target = target;
        m_origin = origin;
    }

    void stop() {
        m_viewport = NULL;
        m_target.clear();
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

    /** Rotation origin point, in widget coordinates. */
    QPointF m_origin;

    /** Head of the rotation item, in scene coordinates. */
    QPointF m_sceneHead;

    /** Bounding rect of this item. */
    QRectF m_boundingRect;
};


RotationInstrument::RotationInstrument(QObject *parent):
    DragProcessingInstrument(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease), parent)
{}

RotationInstrument::~RotationInstrument() {
    ensureUninstalled();
}

bool RotationInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    if(!(event->modifiers() & Qt::AltModifier))
        return false;

    //QnResourceWidget *widget = 

    dragProcessor()->mousePressEvent(viewport, event);

    return false;
}

void RotationInstrument::startDrag(DragInfo *info) {

}

void RotationInstrument::dragMove(DragInfo *info) {

}

void RotationInstrument::finishDrag(DragInfo *info) {

}

