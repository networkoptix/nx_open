#include "motionselectioninstrument.h"
#include <cassert>
#include <QGraphicsObject>
#include <QMouseEvent>
#include <utils/common/scoped_painter_rollback.h>
#include <ui/graphics/items/resource_widget.h>

class MotionSelectionItem: public QGraphicsObject {
public:
    MotionSelectionItem(): 
        QGraphicsObject(NULL),
        m_viewport(NULL)
    {
        setAcceptedMouseButtons(0);

        /* Don't disable this item here or it will swallow mouse wheel events. */
    }

    virtual QRectF boundingRect() const override {
        return QRectF(m_origin, m_corner).normalized();
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) override {
        if (widget != m_viewport)
            return; /* Draw it on source viewport only. */

        QnScopedPainterPenRollback penRollback(painter, QPen(QColor(0, 255, 0, 32), 0));
        QnScopedPainterBrushRollback brushRollback(painter, QColor(0, 255, 0, 16));
        painter->drawRect(boundingRect());
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

    void setOrigin(const QPointF &origin) {
        prepareGeometryChange();
        m_origin = origin;
    }

    void setCorner(const QPointF &corner) {
        prepareGeometryChange();
        m_corner = corner;
    }

    const QPointF &origin() const {
        return m_origin;
    }

    const QPointF &corner() const {
        return m_corner;
    }

private:
    /** Viewport that this selection item will be drawn at. */
    QWidget *m_viewport;

    /** Origin of the selection item, in parent coordinates. */
    QPointF m_origin;

    /** Second corner of the selection item, in parent coordinates. */
    QPointF m_corner;
};

MotionSelectionInstrument::MotionSelectionInstrument(QObject *parent):
    base_type(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent)
{}

MotionSelectionInstrument::~MotionSelectionInstrument() {
    ensureUninstalled();
}

void MotionSelectionInstrument::installedNotify() {
    assert(selectionItem() == NULL);

    ensureSelectionItem();

    base_type::installedNotify();
}

void MotionSelectionInstrument::aboutToBeUninstalledNotify() {
    base_type::aboutToBeUninstalledNotify();

    if(selectionItem() != NULL)
        delete selectionItem();
}

void MotionSelectionInstrument::ensureSelectionItem() {
    if(selectionItem() != NULL)
        return;

    m_selectionItem = new MotionSelectionItem();
    selectionItem()->setVisible(false);
    if(scene() != NULL)
        scene()->addItem(selectionItem());
}

bool MotionSelectionInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    if(!(event->modifiers() & Qt::ShiftModifier))
        return false;

    QGraphicsView *view = this->view(viewport);
    QnResourceWidget *target = this->item<QnResourceWidget>(view, event->pos());
    if(target == NULL)
        return false;

    m_target = target;
    
    dragProcessor()->mousePressEvent(viewport, event);
    
    event->accept();
    return false;
}

bool MotionSelectionInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return false;
    }

    return base_type::paintEvent(viewport, event);
}

void MotionSelectionInstrument::startDragProcess(DragInfo *info) {
    emit selectionProcessStarted(info->view(), target());
}

void MotionSelectionInstrument::startDrag(DragInfo *info) {
    m_selectionStartedEmitted = false;

    if(target() == NULL) {
        /* Whoops, already destroyed. */
        dragProcessor()->reset();
        return;
    }

    ensureSelectionItem();
    selectionItem()->setParentItem(target());
    selectionItem()->setOrigin(target()->mapFromMotionGrid(target()->mapToMotionGrid(target()->mapFromScene(info->mousePressScenePos()))));
    selectionItem()->setViewport(info->view()->viewport());
    selectionItem()->setVisible(true);

    emit selectionStarted(info->view(), target());
    m_selectionStartedEmitted = true;
}

void MotionSelectionInstrument::dragMove(DragInfo *info) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return;
    }

    ensureSelectionItem();
    selectionItem()->setCorner(target()->mapFromMotionGrid(target()->mapToMotionGrid(target()->mapFromScene(info->mouseScenePos()))));
}

void MotionSelectionInstrument::finishDrag(DragInfo *info) {
    if(!m_selectionStartedEmitted)
        emit selectionFinished(info->view(), target());
    
    ensureSelectionItem();
    if(target() != NULL) {
        /* Qt handles QRect borders in totally inhuman way, so we have to do everything by hand. */
        QPoint o = target()->mapToMotionGrid(selectionItem()->origin());
        QPoint c = target()->mapToMotionGrid(selectionItem()->corner());
        target()->addToMotionSelection(QRect(
            QPoint(qMin(o.x(), c.x()), qMin(o.y(), c.y())),
            QSize(qAbs(o.x() - c.x()), qAbs(o.y() - c.y()))
        ));
    }

    selectionItem()->setVisible(false);
    selectionItem()->setParentItem(NULL);
}

void MotionSelectionInstrument::finishDragProcess(DragInfo *info) {
    emit selectionProcessFinished(info->view(), target());
}





