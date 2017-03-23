#pragma once

#include <QtWidgets/QGraphicsScene>

class QnGraphicsScene: public QGraphicsScene
{
    Q_OBJECT
    using base_type = QGraphicsScene;

public:
    QnGraphicsScene(QObject *parent = 0);
    virtual ~QnGraphicsScene();

protected:
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
};
