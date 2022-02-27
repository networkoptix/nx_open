// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "redirecting_instrument.h"

#include <nx/utils/log/assert.h>

void RedirectingInstrument::init(Instrument *target) {
    NX_ASSERT(target);
    m_target = target;
}

bool RedirectingInstrument::event(QGraphicsScene *scene, QEvent *event) {
    if(m_target != nullptr) {
        return m_target->event(scene, event);
    } else {
        return false;
    }
}

bool RedirectingInstrument::event(QGraphicsView *view, QEvent *event) {
    if(m_target != nullptr) {
        return m_target->event(view, event);
    } else {
        return false;
    }
}

bool RedirectingInstrument::event(QWidget *viewport, QEvent *event) {
    if(m_target != nullptr) {
        return m_target->event(viewport, event);
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

