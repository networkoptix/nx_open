#include "tool_tip_instrument.h"

#include <QtGui/QHelpEvent>
#include <QtGui/QGraphicsProxyWidget>

#include <ui/graphics/items/generic/tool_tip_slider.h>
#include <ui/graphics/items/standard/graphics_tooltip.h>
#include <ui/common/tool_tip_queryable.h>

namespace {
    QString widgetToolTip(QWidget *widget, const QPoint &pos) {
        QWidget *childWidget = widget->childAt(pos);
        if(!childWidget)
            childWidget = widget;

        while(true) {
            if(QnToolTipQueryable *queryable = dynamic_cast<QnToolTipQueryable *>(childWidget))
                return queryable->toolTipAt(childWidget->mapFrom(widget, pos));

            if(!childWidget->toolTip().isEmpty() || childWidget == widget)
                return childWidget->toolTip();

            childWidget = childWidget->parentWidget();
        }
    }

    QString itemToolTip(QGraphicsItem *item, const QPointF &pos) {
        if(QnToolTipQueryable *queryable = dynamic_cast<QnToolTipQueryable *>(item))
            return queryable->toolTipAt(pos);

        if(QGraphicsProxyWidget *proxyWidget = dynamic_cast<QGraphicsProxyWidget *>(item))
            if(QWidget *widget = proxyWidget->widget())
                return widgetToolTip(widget, pos.toPoint());
        
        return item->toolTip();
    }

} // anonymous namespace


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

    // TODO: too many dynamic_casts here. Implement hash-based solution?

    QGraphicsItem *targetItem = NULL;
    foreach(QGraphicsItem *item, scene()->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder, view->viewportTransform())) {
        if(dynamic_cast<QnToolTipSlider *>(item)) //TODO: #gdm Bug #1072, fix later
            continue;

        if(!item->toolTip().isEmpty() || dynamic_cast<QnToolTipQueryable *>(item) || dynamic_cast<QGraphicsProxyWidget *>(item)) {
            targetItem = item;
            break;
        }
    }

    if(!targetItem) {
        helpEvent->ignore();
        return true; /* Eat it anyway. */
    }

    GraphicsTooltip::showText(
        itemToolTip(targetItem, targetItem->mapFromScene(scenePos)), 
        targetItem, 
        scenePos, 
        QRectF(view->mapToScene(viewport->geometry().topLeft()), view->mapToScene(viewport->geometry().bottomRight()))
    );

    helpEvent->accept();
    return true;
}


