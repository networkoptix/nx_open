#include "widget_layout_instrument.h"

#include <ui/animation/animation_event.h>
#include <ui/graphics/items/standard/graphics_widget.h>

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
