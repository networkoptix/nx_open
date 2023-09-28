// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "focus_listener_instrument.h"

#include <ui/animation/animation_event.h>

FocusListenerInstrument::FocusListenerInstrument(QObject *parent):
    Instrument(Viewport, makeSet(AnimationEvent::Animation), parent),
    m_focusItem(nullptr)
{}

FocusListenerInstrument::~FocusListenerInstrument() {
    return;
}

bool FocusListenerInstrument::animationEvent(AnimationEvent *) {
    setFocusItem(scene()->focusItem());

    return false;
}

void FocusListenerInstrument::enabledNotify() {
    setFocusItem(scene()->focusItem());
}

void FocusListenerInstrument::aboutToBeDisabledNotify() {
    setFocusItem(nullptr);
}

void FocusListenerInstrument::setFocusItem(QGraphicsItem *focusItem) {
    if(m_focusItem == focusItem)
        return;

    m_focusItem = focusItem;

    emit focusItemChanged();
}
