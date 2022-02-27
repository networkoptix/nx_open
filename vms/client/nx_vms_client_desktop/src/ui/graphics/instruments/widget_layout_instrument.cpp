// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "widget_layout_instrument.h"

#include <ui/animation/animation_event.h>
#include <qt_graphics_items/graphics_widget.h>

WidgetLayoutInstrument::WidgetLayoutInstrument(QObject *parent):
    Instrument(Viewport, makeSet(AnimationEvent::Animation), parent)
{}

WidgetLayoutInstrument::~WidgetLayoutInstrument() {
    return;
}

bool WidgetLayoutInstrument::animationEvent(AnimationEvent *) {
    /* Layout everything before the paint event is delivered. */
    GraphicsWidget::handlePendingLayoutRequests(scene());

    return false;
}
