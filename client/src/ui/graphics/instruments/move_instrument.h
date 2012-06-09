#ifndef QN_MOVE_INSTRUMENT_H
#define QN_MOVE_INSTRUMENT_H

#include <QPoint>
#include "drag_processing_instrument.h"
#include <ui/common/weak_graphics_item_pointer.h>

class MoveInstrument: public DragProcessingInstrument {
    Q_OBJECT;
public:
    MoveInstrument(QObject *parent);

    virtual ~MoveInstrument();

    /**
     * Whether the instrument is effective.
     * 
     * When dragging instrument is not effective it never starts the actual
     * drag. However, drag process is still handled and <tt>moveProcessStarted</tt>
     * signal is emitted.
     */
    bool isEffective() const {
        return m_effective;
    }

    void setEffective(bool effective) {
        m_effective = effective;
    }

signals:
    void moveProcessStarted(QGraphicsView *view);
    void moveStarted(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void move(QGraphicsView *view, const QPointF &totalDelta);
    void moveFinished(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void moveProcessFinished(QGraphicsView *view);

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private:
    QGraphicsItem *draggedItem() const {
        return m_draggedItem.data();
    }

    void moveItem(QGraphicsItem *item, const QPointF &sceneDeltaPos) const;

private:
    bool m_effective;
    bool m_moveStartedEmitted;
    WeakGraphicsItemPointer m_draggedItem;
    WeakGraphicsItemPointerList m_draggedItems;
};

#endif // QN_MOVE_INSTRUMENT_H
