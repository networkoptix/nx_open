#include "tool_tip_instrument.h"

#include <QtCore/QAbstractItemModel>
#include <QtGui/QHelpEvent>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QAbstractItemView>

#include <ui/graphics/items/standard/graphics_tool_tip.h>
#include <ui/common/tool_tip_queryable.h>

ToolTipInstrument::ToolTipInstrument(QObject *parent):
    Instrument(Viewport, makeSet(QEvent::ToolTip), parent)
{}

ToolTipInstrument::~ToolTipInstrument() {
    return;
}

void ToolTipInstrument::addIgnoredItem(QGraphicsItem* item)
{
    m_ignoredItems.insert(item);
}

bool ToolTipInstrument::removeIgnoredItem(QGraphicsItem* item)
{
    return m_ignoredItems.remove(item);
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
            if (m_ignoredItems.contains(item))
                return false;
            if (!satisfiesItemConditions(item))
                continue;
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

// TODO: #Elric implement like in help topic accessor, with bubbleUp.
QString ToolTipInstrument::widgetToolTip(QWidget* widget, const QPoint& pos) const
{
    auto childWidget = widget->childAt(pos);
    if (!childWidget)
        childWidget = widget;

    while (true)
    {
        if (auto queryable = dynamic_cast<ToolTipQueryable*>(childWidget))
            return queryable->toolTipAt(childWidget->mapFrom(widget, pos));

        if (auto view = dynamic_cast<QAbstractItemView*>(childWidget))
        {
            if (view->model())
            {
                QVariant toolTip = view->indexAt(childWidget->mapFrom(widget, pos)).
                    data(Qt::ToolTipRole);

                if (toolTip.convert(QVariant::String))
                    return toolTip.toString();
            }
        }

        if (!childWidget->toolTip().isEmpty() || childWidget == widget)
            return childWidget->toolTip();

        childWidget = childWidget->parentWidget();
    }
}

QString ToolTipInstrument::itemToolTip(QGraphicsItem* item, const QPointF& pos) const
{
    if (m_ignoredItems.contains(item))
        return QString();

    if (auto queryable = dynamic_cast<ToolTipQueryable*>(item))
        return queryable->toolTipAt(pos);

    if (auto proxyWidget = dynamic_cast<QGraphicsProxyWidget*>(item))
        if (auto widget = proxyWidget->widget())
            return widgetToolTip(widget, pos.toPoint());

    return item->toolTip();
}
