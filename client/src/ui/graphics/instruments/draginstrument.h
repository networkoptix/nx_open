#ifndef QN_DRAG_INSTRUMENT_H
#define QN_DRAG_INSTRUMENT_H

#include <QPoint>
#include "dragprocessinginstrument.h"

class DragInstrument: public DragProcessingInstrument {
    Q_OBJECT;
public:
    DragInstrument(QObject *parent);
    virtual ~DragInstrument();

signals:
    void draggingStarted(QGraphicsView *view, QList<QGraphicsItem *> items);
    void draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items);

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;

    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;

private:
    QGraphicsItem *m_itemToSelect;
};

#endif // QN_DRAG_INSTRUMENT_H
