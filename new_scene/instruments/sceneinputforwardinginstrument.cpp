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

bool SceneInputForwardingInstrument::keyPressEvent(QGraphicsScene *scene, QKeyEvent *event) {
    open(scene)->event(event);
    
    return event->isAccepted();
}

bool SceneInputForwardingInstrument::keyReleaseEvent(QGraphicsScene *scene, QKeyEvent *event) {
    open(scene)->event(event);

    return event->isAccepted();
}

bool SceneInputForwardingInstrument::mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    open(scene)->event(event);

    return event->isAccepted();
}

bool SceneInputForwardingInstrument::mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    open(scene)->event(event);

    return event->isAccepted();
}

bool SceneInputForwardingInstrument::mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    open(scene)->event(event);

    return event->isAccepted();
}

bool SceneInputForwardingInstrument::mouseDoubleClickEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    open(scene)->event(event);

    return event->isAccepted();
}

