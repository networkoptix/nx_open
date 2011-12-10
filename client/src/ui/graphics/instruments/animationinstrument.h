#ifndef QN_ANIMATION_INSTRUMENT_H
#define QN_ANIMATION_INSTRUMENT_H

#include "instrument.h"
#include <ui/animation/animation_event.h>

class AnimationInstrument: public Instrument {
    Q_OBJECT;
public:
    AnimationInstrument(QObject *parent = NULL):
        Instrument(VIEWPORT, makeSet(AnimationEvent::Animation), parent)
    {}

    AnimationTimer *animationTimer() {
        return Instrument::animationTimer();
    }
};

#endif // QN_ANIMATION_INSTRUMENT_H
