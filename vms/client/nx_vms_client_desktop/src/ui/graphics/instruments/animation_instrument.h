// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_ANIMATION_INSTRUMENT_H
#define QN_ANIMATION_INSTRUMENT_H

#include <ui/animation/animation_event.h>

#include "instrument.h"

/**
 * Instrument that is used internally by instrument manager to maintain a
 * single animation timer instance.
 */
class AnimationInstrument: public Instrument {
    Q_OBJECT;
public:
    AnimationInstrument(QObject *parent = nullptr):
        Instrument(Viewport, makeSet(AnimationEvent::Animation), parent)
    {}

    AnimationTimer *animationTimer() {
        return Instrument::animationTimer();
    }
};

#endif // QN_ANIMATION_INSTRUMENT_H
