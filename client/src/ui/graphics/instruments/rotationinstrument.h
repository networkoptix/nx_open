#ifndef QN_ROTATION_INSTRUMENT_H
#define QN_ROTATION_INSTRUMENT_H

#include "dragprocessinginstrument.h"

class QnRotationInstrument: public Instrument {
public:
    QnRotationInstrument(QObject *parent = NULL);
    virtual ~QnRotationInstrument();

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    /*virtual void startDrag() override;
    virtual void drag() override;
    virtual void finishDrag() override;*/
};

#endif // QN_ROTATION_INSTRUMENT_H
