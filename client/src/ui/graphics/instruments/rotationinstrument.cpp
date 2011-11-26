#include "rotationinstrument.h"
#include <cassert>
#include <cmath>
#include <QMouseEvent>
#include <QGraphicsObject>
#include <ui/graphics/items/resource_widget.h>

class RotationItem: public QGraphicsObject {
public:
    RotationItem(QGraphicsItem *parent = NULL): 
        QGraphicsObject(parent)
    {

    }

    virtual QRectF boundingRect() const override {
        if (m_viewport == NULL)
            return QRectF();

        ensureRect();

        return m_boundingRect;
    }

    /* We can also implement shape(), but most probably there is no point. */

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) override {
        if (widget != m_viewport)
            return; /* Draw it on source viewport only. */

        assert(target() != NULL);

#if 0
        QPointF sceneOrigin = target()->mapToScene(m_origin);
        if(!qFuzzyCompare(sceneOrigin, m_sceneOrigin)) {
            m_sceneOrigin = sceneOrigin;
            updateRect();
        }

        static QColor color1(255, 0, 0, 250);
        static QColor color2(255, 0, 0, 100);

        static const int r = 30;
        static const int penWidth = 6;
        static const int p_line_len = 220;
        static const int arrowSize = 30;

        painter->save();

        painter->setPen(QPen(color2, penWidth, Qt::SolidLine));
        painter->drawLine(m_sceneOrigin, m_sceneHead);

        // building new line
        QLineF line(m_sceneOrigin, m_sceneHead);
        QLineF line_p = line.unitVector().normalVector();
        line_p.setLength(p_line_len/2);

        line_p = QLineF(line_p.p2(),line_p.p1());
        line_p.setLength(p_line_len);

        painter->drawLine(line_p);

        double angle = ::acos(line_p.dx() / line_p.length());
        if (line_p.dy() >= 0)
            angle = 2 * M_PI - angle;

        qreal s = 2.5;

        QPointF sourceArrowP1 = line_p.p1() + QPointF(sin(angle + M_PI / s) * arrowSize, cos(angle + M_PI / s) * arrowSize);
        QPointF sourceArrowP2 = line_p.p1() + QPointF(sin(angle + M_PI - M_PI / s) * arrowSize, cos(angle + M_PI - M_PI / s) * arrowSize);
        QPointF destArrowP1 = line_p.p2() + QPointF(sin(angle - M_PI / s) * arrowSize, cos(angle - M_PI / s) * arrowSize);
        QPointF destArrowP2 = line_p.p2() + QPointF(sin(angle - M_PI + M_PI / s) * arrowSize, cos(angle - M_PI + M_PI / s) * arrowSize);

        painter->setBrush(color2);
        painter->drawPolygon(QPolygonF() << line_p.p1() << sourceArrowP1 << sourceArrowP2);
        painter->drawPolygon(QPolygonF() << line_p.p2() << destArrowP1 << destArrowP2);

        painter->restore();
#endif
    }

    void start(QWidget *viewport, QnResourceWidget *target, const QPointF &origin, qreal originAngle) {
        m_origin = origin;
        updateRect();
    }

    void stop() {
        m_viewport = NULL;
        m_target.clear();

        updateRect();
    }

    void setHead(const QPointF &head) {
        m_sceneHead = head;

        updateRect();
    }

    const QPointF &origin() const {
        return m_origin;
    }

    const QPointF &head() const {
        return m_sceneHead;
    }

    /**
     * Sets this item's viewport. This item will be drawn only on the given
     * viewport. This item won't access the given viewport in any way, so it is
     * safe to delete the viewport without notifying the item.
     * 
     * \param viewport                  Viewport to draw this item on.
     */
    void setViewport(QWidget *viewport) {
        prepareGeometryChange();
        m_viewport = viewport;
    }

    /**
     * \returns                         This rubber band item's viewport.
     */
    QWidget *viewport() const {
        return m_viewport;
    }

    QnResourceWidget *target() const {
        return m_target.data();
    }

protected:
    void updateRect() {
        prepareGeometryChange();

        m_cacheDirty = true;
    }

    void ensureRect() const {
        if (!m_cacheDirty)
            return;

        if(m_viewport == NULL || m_target.isNull()) {
            m_boundingRect = QRectF();
            m_cacheDirty = false;
            return;
        }

        
    }

private:
    /** Viewport that this rotation item will be drawn at. */
    QWidget *m_viewport;

    /** Widget being rotated. */
    QWeakPointer<QnResourceWidget> m_target;

    /** Rotation origin point, in widget coordinates. */
    QPointF m_origin;

    /** Rotation origin point, in scene coordinates. */
    QPointF m_sceneOrigin;

    /** Rotation reference angle, in widget coordinates. */
    qreal m_originAngle;

    /** Head of the rotation item, in scene coordinates. */
    QPointF m_sceneHead;

    /** Whether stored rectangles need recalculating. */
    mutable bool m_cacheDirty;

    /** Item's bounding rect, in scene coordinates. */
    mutable QRectF m_boundingRect;

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

