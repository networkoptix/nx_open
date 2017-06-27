#include "drag_processing_instrument.h"

#include <QtCore/QTimer>

#include <QtGui/QMouseEvent>

#include <QtWidgets/QGraphicsSceneMouseEvent>

void DragProcessingInstrument::initialize()
{
    DragProcessor *processor = new DragProcessor(this);
    processor->setHandler(this);
}

void DragProcessingInstrument::reset()
{
    dragProcessor()->reset();
}

void DragProcessingInstrument::resetLater()
{
    QTimer::singleShot(1, this, &DragProcessingInstrument::reset);
}

void DragProcessingInstrument::aboutToBeDisabledNotify()
{
    dragProcessor()->reset();
}

bool DragProcessingInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event)
{
    dragProcessor()->mousePressEvent(viewport, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::mouseDoubleClickEvent(QWidget* viewport, QMouseEvent* event)
{
    dragProcessor()->mousePressEvent(viewport, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event)
{
    dragProcessor()->mouseMoveEvent(viewport, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event)
{
    dragProcessor()->mouseReleaseEvent(viewport, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::paintEvent(QWidget *viewport, QPaintEvent *event)
{
    dragProcessor()->paintEvent(viewport, event);
    return false;
}

bool DragProcessingInstrument::mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event)
{
    dragProcessor()->mousePressEvent(scene, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::mouseDoubleClickEvent(QGraphicsScene* scene, QGraphicsSceneMouseEvent* event)
{
    dragProcessor()->mousePressEvent(scene, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event)
{
    dragProcessor()->mouseMoveEvent(scene, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event)
{
    dragProcessor()->mouseReleaseEvent(scene, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event)
{
    dragProcessor()->mousePressEvent(item, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::mouseDoubleClickEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event)
{
    dragProcessor()->mousePressEvent(item, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event)
{
    dragProcessor()->mouseMoveEvent(item, event);
    event->accept();
    return false;
}

bool DragProcessingInstrument::mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event)
{
    dragProcessor()->mouseReleaseEvent(item, event);
    event->accept();
    return false;
}
