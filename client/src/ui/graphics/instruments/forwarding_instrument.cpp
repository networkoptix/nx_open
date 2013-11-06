#include "forwarding_instrument.h"

#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsItem>

#include <ui/graphics/instruments/instrument_manager.h>

namespace {
    template<class Base>
    class Object: public Base {
    public:
        bool processEvent(QEvent *event) {
            return this->event(event);
        }

        bool staticProcessEvent(QEvent *event) {
            return this->Base::event(event);
        }
    };

    template<class Base>
    Object<Base> *open(Base *object) {
        return static_cast<Object<Base> *>(object);
    }

    class GraphicsItem: public QGraphicsItem {
    public:
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
    /* Some of the viewport's events are handled by the view via event filters.
     * As we employ the same mechanism, these events won't get delivered unless we
     * forward them here. 
     * 
     * Yes, this is an evil hack. */
    bool filtered = false;
    QGraphicsView *view = this->view(viewport);
    switch (event->type()) {
    case QEvent::Resize:
    case QEvent::Paint:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::MouseMove:
    case QEvent::ContextMenu:
    case QEvent::Wheel:
    case QEvent::Drop:
    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::DragLeave:
        // All animations and paint events should be disabled while in fullscreen-transition process on MacOSX
        if (!manager()->isAnimationEnabled())
            return false;
        filtered = open(static_cast<QFrame *>(view))->staticProcessEvent(event);
        break;
    case QEvent::LayoutRequest:
    case QEvent::Gesture:
    case QEvent::GestureOverride:
        filtered = open(view)->processEvent(event);
        break;
    default:
        break;
    }

    if(!filtered)
        open(viewport)->processEvent(event);

    return false;
}

bool ForwardingInstrument::sceneEvent(QGraphicsItem *item, QEvent *event) {
    QGraphicsObject *object = item->toGraphicsObject();
    if(object != NULL)
        if(open(static_cast<QObject *>(object))->processEvent(event))
            return false;

    open(item)->processSceneEvent(event);
    return false;
}
