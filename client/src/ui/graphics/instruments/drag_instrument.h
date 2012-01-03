#ifndef QN_DRAG_INSTRUMENT_H
#define QN_DRAG_INSTRUMENT_H

#include <QPoint>
#include "drag_processing_instrument.h"

class DragInstrument: public DragProcessingInstrument {
    Q_OBJECT;
public:
    DragInstrument(QObject *parent);

    virtual ~DragInstrument();

    /**
     * Whether the instrument is effective.
     * 
     * When dragging instrument is not effective it never starts the actual
     * drag. However, drag process is still handled and <tt>dragProcessStarted</tt>
     * signal is emitted.
     */
    bool isEffective() const {
        return m_effective;
    }

    void setEffective(bool effective) {
        m_effective = effective;
    }

signals:
    void dragProcessStarted(QGraphicsView *view);
    void dragStarted(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void drag(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void dragFinished(QGraphicsView *view, const QList<QGraphicsItem *> &items);
    void dragProcessFinished(QGraphicsView *view);

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private:
    bool m_effective;
    QGraphicsItem *m_itemToSelect;
};

#endif // QN_DRAG_INSTRUMENT_H
