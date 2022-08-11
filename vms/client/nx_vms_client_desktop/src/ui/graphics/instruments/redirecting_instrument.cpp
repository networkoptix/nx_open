// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "redirecting_instrument.h"

#include <nx/utils/log/assert.h>

void RedirectingInstrument::init(Instrument *target) {
    NX_ASSERT(target);
    m_target = target;
}

bool RedirectingInstrument::anyEvent(QGraphicsScene *scene, QEvent *event) {
    if(m_target != nullptr) {
        return m_target->anyEvent(scene, event);
    } else {
        return false;
    }
}

bool RedirectingInstrument::anyEvent(QGraphicsView *view, QEvent *event) {
    if(m_target != nullptr) {
        return m_target->anyEvent(view, event);
    } else {
        return false;
    }
}

bool RedirectingInstrument::anyEvent(QWidget *viewport, QEvent *event) {
    if(m_target != nullptr) {
        return m_target->anyEvent(viewport, event);
    } else {
        return false;
    }
}

bool RedirectingInstrument::sceneEvent(QGraphicsItem *item, QEvent *event) {
    if(m_target != nullptr) {
        return m_target->sceneEvent(item, event);
    } else {
        return false;
    }
}
