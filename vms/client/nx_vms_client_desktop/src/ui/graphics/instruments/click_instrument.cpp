// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "click_instrument.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsSceneMouseEvent>

#include <nx/utils/log/assert.h>
#include <ui/common/weak_graphics_item_pointer.h>
#include <ui/common/weak_pointer.h>

namespace {

ClickInfo info(const QGraphicsSceneMouseEvent* src)
{
    return {src->scenePos(), src->button(), src->modifiers()};
}

} // namespace


class ClickInfoPrivate: public QGraphicsSceneMouseEvent {};

struct ClickInstrument::ClickData
{
    ClickData(): item(nullptr) {}

    ClickInfo info;
    WeakGraphicsItemPointer item;
    QPointer<QGraphicsScene> scene;
    QPointer<QGraphicsView> view;
};

ClickInstrument::ClickInstrument(
    Qt::MouseButtons buttons,
    int clickDelayMSec,
    WatchedType watchedType,
    QObject* parent)
    :
    DragProcessingInstrument(
        watchedType,
        makeSet(
            QEvent::GraphicsSceneMousePress,
            QEvent::GraphicsSceneMouseMove,
            QEvent::GraphicsSceneMouseRelease,
            QEvent::GraphicsSceneMouseDoubleClick),
        parent
    ),
    m_buttons(buttons),
    m_clickDelayMSec(clickDelayMSec),
    m_clickTimer(0),
    m_clickData(new ClickData()),
    m_isClick(false),
    m_isDoubleClick(false),
    m_nextDoubleClickIsClick(false)
{
    if (clickDelayMSec < 0)
    {
        NX_ASSERT(false, "Invalid click delay '%1'.", clickDelayMSec);
        m_clickDelayMSec = 0;
    }

    qRegisterMetaType<ClickInfo>();
}

ClickInstrument::~ClickInstrument() {
    ensureUninstalled();
}

void ClickInstrument::aboutToBeDisabledNotify() {
    killClickTimer();

    base_type::aboutToBeDisabledNotify();
}

void ClickInstrument::timerEvent(QTimerEvent*)
{
    QScopedValueRollback<bool> guard(m_isClick, true);
    if (m_isDoubleClick)
    {
        m_isDoubleClick = false;
    }
    else
    {
        m_nextDoubleClickIsClick = true;
    }
    killClickTimer();

    if (watches(Item))
        emitSignals(m_clickData->view.data(), m_clickData->item.data(), m_clickData->info);
    else
        emitSignals(m_clickData->view.data(), m_clickData->scene.data(), m_clickData->info);
}

void ClickInstrument::storeClickData(
    QGraphicsView* view,
    QGraphicsItem* item,
    QGraphicsSceneMouseEvent* event)
{
    m_clickData->info = info(event);
    m_clickData->item = item;
    m_clickData->scene.clear();
    m_clickData->view = view;
}

void ClickInstrument::storeClickData(
    QGraphicsView* view,
    QGraphicsScene* scene,
    QGraphicsSceneMouseEvent* event)
{
    m_clickData->info = info(event);
    m_clickData->item = nullptr;
    m_clickData->scene = scene;
    m_clickData->view = view;
}

void ClickInstrument::killClickTimer() {
    if(m_clickTimer == 0)
        return;

    killTimer(m_clickTimer);
    m_clickTimer = 0;
}

void ClickInstrument::restartClickTimer() {
    killClickTimer();

    m_clickTimer = startTimer(m_clickDelayMSec);
}

template<class T>
bool ClickInstrument::mousePressEventInternal(T *object, QGraphicsSceneMouseEvent *event) {
    if (!(event->button() & m_buttons))
        return false;

    m_isClick = true;
    m_isDoubleClick = false;
    m_nextDoubleClickIsClick = false;

    /* Note that qobject_cast is for a reason here. There are no guarantees
     * regarding the widget stored in the supplied event. */
    QGraphicsView *view = nullptr;
    if(event->widget() != nullptr)
        view = qobject_cast<QGraphicsView *>(event->widget()->parent());
    emitInitialSignal(view, object, info(event));

    dragProcessor()->mousePressEvent(object, event);
    return false;
}

template<class T>
bool ClickInstrument::mouseDoubleClickEventInternal(T *object, QGraphicsSceneMouseEvent *event) {
    if(!(event->button() & m_buttons))
        return false;

    m_isClick = true;
    m_isDoubleClick = !m_nextDoubleClickIsClick;
    m_nextDoubleClickIsClick = false;

    dragProcessor()->mousePressEvent(object, event);
    return false;
}

template<class T>
bool ClickInstrument::mouseMoveEventInternal(T *object, QGraphicsSceneMouseEvent *event) {
    if (!m_isClick)
        return false;

    dragProcessor()->mouseMoveEvent(object, event);
    return false;
}

template<class T>
bool ClickInstrument::mouseReleaseEventInternal(T *object, QGraphicsSceneMouseEvent *event) {
    if(!(event->button() & m_buttons))
        return false;

    if(!m_isClick)
        return false;

    /* Object may get deleted in click signal handlers, so we have to be careful. */
    WeakPointer<T> guard(object);

    /* Note that qobject_cast is for a reason here. There are no guarantees
     * regarding the widget stored in the supplied event. */
    QGraphicsView *view = nullptr;
    if(event->widget() != nullptr)
        view = qobject_cast<QGraphicsView *>(event->widget()->parent());
    if(m_clickDelayMSec != 0) {
        if(m_isDoubleClick) {
            killClickTimer();
            emitSignals(view, object, info(event));
        } else {
            storeClickData(view, object, event);
            restartClickTimer();
        }
    } else {
        emitSignals(view, object, info(event));
    }

    m_isClick = false;
    m_isDoubleClick = false;

    if(guard) {
        dragProcessor()->mouseReleaseEvent(object, event);
    } else {
        dragProcessor()->reset();
    }

    return false;
}

void ClickInstrument::emitSignals(QGraphicsView* view, QGraphicsItem* item, ClickInfo info)
{
    // May have been destroyed.
    if (view == nullptr || item == nullptr)
        return;

    if (m_isDoubleClick)
        emit itemDoubleClicked(view, item, info);
    else
        emit itemClicked(view, item, info);
}

void ClickInstrument::emitSignals(QGraphicsView* view, QGraphicsScene* scene, ClickInfo info)
{
    // May have been destroyed.
    if (view == nullptr || scene == nullptr)
        return;

    if (m_isDoubleClick)
        emit sceneDoubleClicked(view, info);
    else
        emit sceneClicked(view, info);
}

void ClickInstrument::emitInitialSignal(QGraphicsView* view, QGraphicsItem* item, ClickInfo info)
{
    emit itemPressed(view, item, info);
}

void ClickInstrument::emitInitialSignal(QGraphicsView* view, QGraphicsScene*, ClickInfo info)
{
    emit scenePressed(view, info);
}

void ClickInstrument::startDrag(DragInfo *) {
    dragProcessor()->reset();
}

void ClickInstrument::finishDragProcess(DragInfo *) {
    m_isClick = false;
    m_isDoubleClick = false;
}

bool ClickInstrument::mousePressEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event)
{
    return mousePressEventInternal(item, event);
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event)
{
    return mouseDoubleClickEventInternal(item, event);
}

bool ClickInstrument::mouseMoveEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event)
{
    return mouseMoveEventInternal(item, event);
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event)
{
    return mouseReleaseEventInternal(item, event);
}

bool ClickInstrument::mousePressEvent(QGraphicsScene* scene, QGraphicsSceneMouseEvent* event)
{
    return mousePressEventInternal(scene, event);
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsScene* scene, QGraphicsSceneMouseEvent* event)
{
    return mouseDoubleClickEventInternal(scene, event);
}

bool ClickInstrument::mouseMoveEvent(QGraphicsScene* scene, QGraphicsSceneMouseEvent* event)
{
    return mouseMoveEventInternal(scene, event);
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsScene* scene, QGraphicsSceneMouseEvent* event)
{
    return mouseReleaseEventInternal(scene, event);
}
