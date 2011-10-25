#ifndef QN_SCENE_INPUT_STOP_INSTRUMENT_H
#define QN_SCENE_INPUT_STOP_INSTRUMENT_H

#include "instrument.h"

class SceneInputStopInstrument: public Instrument {
    Q_OBJECT;
public:
    SceneInputStopInstrument(QObject *parent = NULL);

private:
    virtual bool keyPressEvent(QGraphicsScene *, QKeyEvent *) override { return true; }
    virtual bool keyReleaseEvent(QGraphicsScene *, QKeyEvent *) override { return true; }

    virtual bool mouseMoveEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) { return true; }
    virtual bool mousePressEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) { return true; }
    virtual bool mouseReleaseEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) { return true; }
    virtual bool mouseDoubleClickEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) { return true; }
};

#endif // QN_SCENE_INPUT_STOP_INSTRUMENT_H
