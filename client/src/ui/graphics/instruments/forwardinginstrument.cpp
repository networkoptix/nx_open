#include "forwardinginstrument.h"
#include <QGraphicsScene>
#include <QGraphicsItem>

namespace {
    class Object: public QObject {
    public:
        friend class ForwardingInstrument;

        bool processEvent(QEvent *event) {
            return this->event(event);
        }
    };

    Object *open(QObject *object) {
        return static_cast<Object *>(object);
    }

    class GraphicsItem: public QGraphicsItem {
    public:
        friend class ForwardingInstrument;

        bool processSceneEvent(QEvent *event) {
            return this->sceneEvent(event);
        }
    };

    GraphicsItem *open(QGraphicsItem *item) {
        return static_cast<GraphicsItem *>(item);
    }

} // anonymous namespace

bool ForwardingInstrument::event(QGraphicsScene *scene, QEvent *event) {
    open(scene)->processEvent(event);

    return false;
}

bool ForwardingInstrument::event(QGraphicsView *view, QEvent *event) {
    open(view)->processEvent(event);

    return false;
}

bool ForwardingInstrument::event(QWidget *viewport, QEvent *event) {
    open(viewport)->processEvent(event);

    return false;
}

bool ForwardingInstrument::sceneEvent(QGraphicsItem *item, QEvent *event) {
    open(item)->processSceneEvent(event);

    return false;
}
