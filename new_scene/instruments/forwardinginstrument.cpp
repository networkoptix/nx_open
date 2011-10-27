#include "forwardinginstrument.h"
#include <QGraphicsScene>
#include <QGraphicsItem>

namespace {
    class Object: public QObject {
        friend class ForwardingInstrument;
    };

    Object *open(QObject *object) {
        return static_cast<Object *>(object);
    }

    class GraphicsItem: public QGraphicsItem {
        friend class ForwardingInstrument;
    };

    GraphicsItem *open(QGraphicsItem *item) {
        return static_cast<GraphicsItem *>(item);
    }

} // anonymous namespace

bool ForwardingInstrument::event(QGraphicsScene *scene, QEvent *event) {
    open(scene)->event(event);

    return event->isAccepted();
}

bool ForwardingInstrument::event(QGraphicsView *view, QEvent *event) {
    open(view)->event(event);

    return event->isAccepted();
}

bool ForwardingInstrument::event(QWidget *viewport, QEvent *event) {
    open(viewport)->event(event);

    return event->isAccepted();
}

bool ForwardingInstrument::sceneEvent(QGraphicsItem *item, QEvent *event) {
    open(item)->sceneEvent(event);

    return event->isAccepted();
}
