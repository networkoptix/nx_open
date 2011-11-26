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
    void dragProcessStarted(QGraphicsView *view);
    void dragStarted(QGraphicsView *view, const QList<QGraphicsItem *> &items);
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
    QGraphicsItem *m_itemToSelect;
};

#endif // QN_DRAG_INSTRUMENT_H
