// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_DRAG_INSTRUMENT_H
#define QN_DRAG_INSTRUMENT_H

#include "drag_processing_instrument.h"
#include <ui/common/weak_graphics_item_pointer.h>

class DragInstrument: public DragProcessingInstrument {
    Q_OBJECT;
public:
    DragInstrument(QObject *parent = nullptr);

    virtual ~DragInstrument();

signals:
    void dragProcessStarted(QGraphicsView *view);
    void dragStarted(QGraphicsView *view);
    void dragFinished(QGraphicsView *view);
    void dragProcessFinished(QGraphicsView *view);

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private:
    WeakGraphicsItemPointer m_itemToSelect;
};


#endif // QN_DRAG_INSTRUMENT_H
