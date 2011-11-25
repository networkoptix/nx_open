#ifndef QN_ROTATION_INSTRUMENT_H
#define QN_ROTATION_INSTRUMENT_H

#include "dragprocessinginstrument.h"

class RotationInstrument: public DragProcessingInstrument {
public:
    RotationInstrument(QObject *parent = NULL);
    virtual ~RotationInstrument();

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
};

#endif // QN_ROTATION_INSTRUMENT_H
