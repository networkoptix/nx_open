#include "sceneinputforwardinginstrument.h"
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QGraphicsSceneMouseEvent>

namespace {
    QEvent::Type sceneEventTypes[] = {
        QEvent::GraphicsSceneMousePress,
        QEvent::GraphicsSceneMouseMove,
        QEvent::GraphicsSceneMouseRelease,
        QEvent::GraphicsSceneMouseDoubleClick
    };

    class Object: public QObject {
        friend class SceneInputForwardingInstrument;
    };

    Object *open(QObject *object) {
        return static_cast<Object *>(object);
    }

} // anonymous namespace

SceneInputForwardingInstrument::SceneInputForwardingInstrument(QObject *parent):
    Instrument(makeSet(sceneEventTypes), makeSet(), makeSet(), false, parent)
{}

bool SceneInputForwardingInstrument::event(QGraphicsScene *scene, QEvent *event) {
    open(scene)->event(event);

    return event->isAccepted();
}

