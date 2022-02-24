// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "graphics_scene.h"

#include <QtWidgets/QGraphicsSceneDragDropEvent>

QnGraphicsScene::QnGraphicsScene(QObject *parent):
    base_type(parent)
{
    setFocusOnTouch(false);
}

QnGraphicsScene::~QnGraphicsScene()
{
}

void QnGraphicsScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    /* #QTBUG // TODO: #vkutin check if this is still relevant
     * There is a bug in Qt5 that results in drops not working if no
     * drag move events were delivered. We work this around by invoking the
     * corresponding handler in drag enter event. */
    base_type::dragEnterEvent(event);
    base_type::dragMoveEvent(event);
    event->accept(); /* Drag enter always accepts the event, so we should re-accept it. */
}
