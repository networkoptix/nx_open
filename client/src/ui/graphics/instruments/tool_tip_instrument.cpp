#include "tool_tip_instrument.h"

#include <QtGui/QHelpEvent>


ToolTipInstrument::ToolTipInstrument(QObject *parent):
    Instrument(Viewport, makeSet(QEvent::ToolTip), parent)
{}

ToolTipInstrument::~ToolTipInstrument() {
    return;
}

bool ToolTipInstrument::event(QWidget *viewport, QEvent *event) {
    if(event->type() != QEvent::ToolTip)
        return base_type::event(viewport, event);

    QGraphicsView *view = this->view(viewport);
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
    QPointF scenePos = view->mapToScene(helpEvent->pos());

    QGraphicsItem *targetItem = NULL;
    foreach(QGraphicsItem *item, scene()->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder, view->viewportTransform())) {
        /* Note that we don't handle proxy widgets separately. */
        if (!item->toolTip().isEmpty()) {
            targetItem = item;
            break;
        }
    }

    if(targetItem) {
        // TODO: create tooltip item here.
    }

    helpEvent->setAccepted(targetItem != NULL);
    return true;
}

