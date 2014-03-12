#ifndef QN_GRAPHICS_SCENE_H
#define QN_GRAPHICS_SCENE_H

#include <QtWidgets/QGraphicsScene>

class QnGraphicsScene: public QGraphicsScene {
    Q_OBJECT
    typedef QGraphicsScene base_type;

public:
    QnGraphicsScene(QObject *parent = 0): base_type(parent) {}
    virtual ~QnGraphicsScene() {}

protected:
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event) {
        /* #QTBUG 
         * There is a bug in Qt5 that results in drops not working if no
         * drag move events were delivered. We work this around by invoking the 
         * corresponding handler in drag enter event. */ // TODO: #Elric check if this is still relevant
        base_type::dragEnterEvent(event);
        base_type::dragMoveEvent(event);
        event->accept(); /* Drag enter always accepts the event, so we should re-accept it. */
    }
};

#endif // QN_GRAPHICS_SCENE_H
