#include "drag_instrument.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QDrag>
#include <QtCore/QMimeData>

#include <core/resource/resource.h>

#include <ui/graphics/items/resource/resource_widget.h>

#include <nx/client/desktop/utils/mime_data.h>

using namespace nx::client::desktop;

DragInstrument::DragInstrument(QObject *parent):
    DragProcessingInstrument(Viewport, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent)
{}

DragInstrument::~DragInstrument() {
    ensureUninstalled();
}

bool DragInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(!dragProcessor()->isWaiting())
        return false;

    QGraphicsView *view = this->view(viewport);
    if (!view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    if(!(event->modifiers() & Qt::ControlModifier))
        return false;

    /* Find the item to drag. */
    QnResourceWidget *target = this->item<QnResourceWidget>(view, event->pos());
    if(target == NULL)
        return false;

    /* Default drag is disabled for videowall items. */
    if (target->resource()->hasFlags(Qn::videowall))
        return false;

    /* If a user ctrl-drags an item that is not selected, we should select it when dragging starts. */
    m_itemToSelect = target;

    dragProcessor()->mousePressEvent(viewport, event);

    event->accept();
    return false;
}

void DragInstrument::startDragProcess(DragInfo *info) {
    emit dragProcessStarted(info->view());
}

void DragInstrument::startDrag(DragInfo* info)
{
    if (!m_itemToSelect.isNull())
    {
        m_itemToSelect.data()->setSelected(true);
        m_itemToSelect.clear();
    }

    QnResourceList resources;
    foreach(QGraphicsItem *item, scene()->selectedItems())
        if (QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item))
            resources.push_back(widget->resource());

    if (resources.isEmpty())
    {
        dragProcessor()->reset();
        return;
    }

    emit dragStarted(info->view());

    MimeData data;
    data.setResources(resources);

    auto mimeData = new QMimeData();
    data.toMimeData(mimeData);

    auto drag = new QDrag(info->view());
    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction, Qt::CopyAction);

    emit dragFinished(info->view());

    dragProcessor()->reset();
}

void DragInstrument::dragMove(DragInfo *) {
    return;
}

void DragInstrument::finishDrag(DragInfo *) {
    return;
}

void DragInstrument::finishDragProcess(DragInfo *info) {
    emit dragProcessFinished(info->view());
}

