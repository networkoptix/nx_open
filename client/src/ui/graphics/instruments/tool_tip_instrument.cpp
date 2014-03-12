#include "tool_tip_instrument.h"

#include <QtCore/QAbstractItemModel>
#include <QtGui/QHelpEvent>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QAbstractItemView>

#include <ui/graphics/items/standard/graphics_tool_tip.h>
#include <ui/common/tool_tip_queryable.h>

namespace {
    QString widgetToolTip(QWidget *widget, const QPoint &pos) { // TODO: #Elric implement like in help topic accessor, with bubbleUp.
        QWidget *childWidget = widget->childAt(pos);
        if(!childWidget)
            childWidget = widget;

        while(true) {
            if(ToolTipQueryable *queryable = dynamic_cast<ToolTipQueryable *>(childWidget))
                return queryable->toolTipAt(childWidget->mapFrom(widget, pos));

            if(QAbstractItemView *view = dynamic_cast<QAbstractItemView *>(childWidget)) {
                if(/*QAbstractItemModel *model = */view->model()) {
                    QVariant toolTip = view->indexAt(childWidget->mapFrom(widget, pos)).data(Qt::ToolTipRole);
                    if (toolTip.convert(QVariant::String))
                        return toolTip.toString();
                }
            }

            if(!childWidget->toolTip().isEmpty() || childWidget == widget)
                return childWidget->toolTip();

            childWidget = childWidget->parentWidget();
        }
    }

    QString itemToolTip(QGraphicsItem *item, const QPointF &pos) {
        if(ToolTipQueryable *queryable = dynamic_cast<ToolTipQueryable *>(item))
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

    /* Note that tooltip is a rare event, so having a lot of dynamic_casts here is OK. */

    QGraphicsItem *targetItem = NULL;
    ToolTipQueryable* targetAsToolTipQueryable = NULL;
    foreach(QGraphicsItem *item, scene()->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder, view->viewportTransform())) {
        if(!item->toolTip().isEmpty() || dynamic_cast<ToolTipQueryable *>(item) || dynamic_cast<QGraphicsProxyWidget *>(item) ) {
            targetItem = item;
            targetAsToolTipQueryable = dynamic_cast<ToolTipQueryable *>(item);
            QGraphicsProxyWidget* targetAsGraphicsProxy = dynamic_cast<QGraphicsProxyWidget *>(item);
            if (!targetAsToolTipQueryable && targetAsGraphicsProxy)
                targetAsToolTipQueryable = dynamic_cast<ToolTipQueryable *>(targetAsGraphicsProxy->widget());
            break;
        }
    }
    if(!targetItem) {
        helpEvent->ignore();
        return true; /* Eat it anyway. */
    }

    if (targetAsToolTipQueryable && targetAsToolTipQueryable->showOwnTooltip(targetItem->mapFromScene(scenePos))) {
        helpEvent->accept();
        return true;
    }

    GraphicsToolTip::showText(
        itemToolTip(targetItem, targetItem->mapFromScene(scenePos)),
        view,
        targetItem,
        helpEvent->pos()
    );

    helpEvent->accept();
    return true;
}


