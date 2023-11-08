// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "drag_instrument.h"

#include <QtCore/QMimeData>
#include <QtGui/QDrag>
#include <QtGui/QMouseEvent>

#include <core/resource/resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <ui/graphics/items/resource/resource_widget.h>

using namespace nx::vms::client::desktop;

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
    if(target == nullptr)
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
    for (auto item: scene()->selectedItems())
    {
        if (auto widget = dynamic_cast<QnResourceWidget*>(item))
            resources.push_back(widget->resource());
    }

    if (resources.isEmpty())
    {
        dragProcessor()->reset();
        return;
    }

    emit dragStarted(info->view());

    MimeData data;
    data.setResources(resources);
    data.addContextInformation(appContext()->mainWindowContext()); //< TODO: #mmalofeev use actual window context.

    auto drag = new QDrag(info->view());
    drag->setMimeData(data.createMimeData());
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
