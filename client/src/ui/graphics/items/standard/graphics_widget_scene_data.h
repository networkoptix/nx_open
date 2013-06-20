
#ifndef GRAPHICS_WIDGET_SCENE_DATA_H
#define GRAPHICS_WIDGET_SCENE_DATA_H

#include <QtCore/QHash>
#include <QtCore/QEvent>
#include <QtCore/QPointF>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsWidget>


class GraphicsWidgetSceneData: public QObject {

    Q_OBJECT
public:
    /** Event type for scene-wide layout requests. */
    static const QEvent::Type HandlePendingLayoutRequests = static_cast<QEvent::Type>(QEvent::User + 0x19FA);

    GraphicsWidgetSceneData(QGraphicsScene *scene, QObject *parent = NULL);

    virtual bool event(QEvent *event) override;

    QGraphicsScene *scene;
    QHash<QGraphicsItem *, QPointF> movingItemsInitialPositions;
    QSet<QGraphicsWidget *> pendingLayoutWidgets;

    QHash<QGraphicsWidget *, const char *> names;
};

//Q_DECLARE_METATYPE(GraphicsWidgetSceneData *);

#endif	//GRAPHICS_WIDGET_SCENE_DATA_H
