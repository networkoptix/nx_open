#ifndef QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H
#define QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H

#include "instrument.h"

class SceneInputForwardingInstrument: public Instrument {
    Q_OBJECT;
public:
    SceneInputForwardingInstrument(QObject *parent = NULL);

protected:
    virtual bool keyPressEvent(QGraphicsScene *, QKeyEvent *) override;
    virtual bool keyReleaseEvent(QGraphicsScene *, QKeyEvent *) override;

    virtual bool mouseMoveEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) override;
    virtual bool mousePressEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) override;
    virtual bool mouseReleaseEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) override;
    virtual bool mouseDoubleClickEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) override;
};

#endif // QN_SCENE_INPUT_FORWARDING_INSTRUMENT_H
