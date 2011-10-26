#ifndef QN_SCENE_INPUT_STOP_INSTRUMENT_H
#define QN_SCENE_INPUT_STOP_INSTRUMENT_H

#include "instrument.h"

class SceneInputStopInstrument: public Instrument {
    Q_OBJECT;
public:
    SceneInputStopInstrument(QObject *parent = NULL);

private:
    virtual bool event(QGraphicsScene *, QEvent *) override { return true; }
};

#endif // QN_SCENE_INPUT_STOP_INSTRUMENT_H
