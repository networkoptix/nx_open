
#include "graphics_widget_scene_data.h"

#include <cassert>

#include "graphics_widget.h"


GraphicsWidgetSceneData::GraphicsWidgetSceneData(QGraphicsScene *scene, QObject *parent):
    QObject(parent),
    scene(scene)
{
    assert(scene);
}

bool GraphicsWidgetSceneData::event(QEvent *event) {
    if(event->type() == HandlePendingLayoutRequests) {
        GraphicsWidget::handlePendingLayoutRequests(scene);
        return true;
    } else {
        return QObject::event(event);
    }
}

Q_DECLARE_METATYPE(GraphicsWidgetSceneData *);
