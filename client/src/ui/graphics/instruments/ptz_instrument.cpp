#include "ptz_instrument.h"

#include <ui/graphics/items/resource/media_resource_widget.h>

PtzInstrument::PtzInstrument(QObject *parent): base_type(Item, makeSet(), parent) {

}

PtzInstrument::~PtzInstrument() {

}

bool PtzInstrument::registeredNotify(QGraphicsItem *item) {
    return base_type::registeredNotify(item) && dynamic_cast<QnMediaResourceWidget *>(item);
}

