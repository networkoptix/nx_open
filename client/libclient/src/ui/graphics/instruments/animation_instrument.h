#ifndef QN_ANIMATION_INSTRUMENT_H
#define QN_ANIMATION_INSTRUMENT_H

#include "instrument.h"
#include <ui/animation/animation_event.h>

/**
 * Instrument that is used internally by instrument manager to maintain a 
 * single animation timer instance.
 */
class AnimationInstrument: public Instrument {
    Q_OBJECT;
public:
    AnimationInstrument(QObject *parent = NULL):
        Instrument(Viewport, makeSet(AnimationEvent::Animation), parent)
    {}

    AnimationTimer *animationTimer() {
        return Instrument::animationTimer();
    }
};

#endif // QN_ANIMATION_INSTRUMENT_H
