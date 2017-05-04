#pragma once

#include <QtCore/QHash>
#include <QtCore/QEvent>
#include <QtCore/QPointF>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QPointer>

#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsWidget>

#include "graphics_widget.h"

class GraphicsWidgetSceneData: public QObject {
    Q_OBJECT;
public:
    /** Event type for scene-wide layout requests. */
    static const QEvent::Type HandlePendingLayoutRequests = static_cast<QEvent::Type>(QEvent::User + 0x19FA);

    GraphicsWidgetSceneData(QGraphicsScene *scene, QObject *parent = NULL):
        QObject(parent),
        scene(scene)
    {
        NX_ASSERT(scene);
    }

    virtual ~GraphicsWidgetSceneData() {
        return;
    }

    virtual bool event(QEvent *event) override {
        if(event->type() == HandlePendingLayoutRequests) {
            if(scene)
                GraphicsWidget::handlePendingLayoutRequests(scene.data());
            return true;
        } else {
            return QObject::event(event);
        }
    }

    QPointer<QGraphicsScene> scene;
    QHash<QGraphicsItem *, QPointF> movingItemsInitialPositions;
    QSet<QGraphicsWidget *> pendingLayoutWidgets;
};

Q_DECLARE_METATYPE(GraphicsWidgetSceneData *);
