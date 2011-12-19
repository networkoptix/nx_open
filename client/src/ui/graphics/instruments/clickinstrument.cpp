#include "clickinstrument.h"
#include <QGraphicsSceneMouseEvent>
#include <QWeakPointer>
#include <QApplication>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/warnings.h>

namespace {
    void copyEvent(const QGraphicsSceneMouseEvent *src, QGraphicsSceneMouseEvent *dst) {
        /* Copy only what is accessible through ClickInfo. */
        dst->setScenePos(src->scenePos());
        dst->setScreenPos(src->screenPos());
        dst->setButtons(src->buttons());
        dst->setButton(src->button());
        dst->setModifiers(src->modifiers());
    }

} // anonymous namespace


class ClickInfoPrivate: public QGraphicsSceneMouseEvent {};

struct ClickInstrument::ClickData {
    ClickData(): item(NULL) {}

    QGraphicsSceneMouseEvent event;
    QGraphicsItem *item;
    QWeakPointer<QGraphicsScene> scene;
    QWeakPointer<QGraphicsView> view;
};

ClickInstrument::ClickInstrument(Qt::MouseButtons buttons, int clickDelayMSec, WatchedType watchedType, QObject *parent): 
    DragProcessingInstrument(
        watchedType,
        makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease, QEvent::GraphicsSceneMouseDoubleClick),
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
    if(clickDelayMSec < 0) {
        qnWarning("Invalid click delay '%1'.", clickDelayMSec);
        m_clickDelayMSec = 0;
    }
}

ClickInstrument::~ClickInstrument() {
    ensureUninstalled();
}

void ClickInstrument::aboutToBeDisabledNotify() {
    killClickTimer();
    
    base_type::aboutToBeDisabledNotify();
}

void ClickInstrument::timerEvent(QTimerEvent *) {
    QnScopedValueRollback<bool> clickRollback(&m_isClick, true);
    if(m_isDoubleClick) {
        m_isDoubleClick = false;
    } else {
        m_nextDoubleClickIsClick = true;
    }
    killClickTimer();

    if(watches(ITEM)) {
        emitSignals(m_clickData->view.data(), m_clickData->item, &m_clickData->event);
    } else {
        emitSignals(m_clickData->view.data(), m_clickData->scene.data(), &m_clickData->event);
    }
}

void ClickInstrument::storeClickData(QGraphicsView *view, QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    copyEvent(event, &m_clickData->event);
    m_clickData->item = item;
    m_clickData->scene.clear();
    m_clickData->view = view;
}

void ClickInstrument::storeClickData(QGraphicsView *view, QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    copyEvent(event, &m_clickData->event);
    m_clickData->item = NULL;
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
    
    /* Note that qobject_cast is for a reason here. There are no guarantees
     * regarding the widget stored in the supplied event. */
    QGraphicsView *view = NULL;
    if(event->widget() != NULL)
        view = qobject_cast<QGraphicsView *>(event->widget()->parent()); 
    if(m_clickDelayMSec != 0) {
        if(m_isDoubleClick) {
            killClickTimer();
            emitSignals(view, object, event);
        } else {
            storeClickData(view, object, event);
            restartClickTimer();
        }
    } else {
        emitSignals(view, object, event);
    }

    m_isClick = false;
    m_isDoubleClick = false;

    dragProcessor()->mouseReleaseEvent(object, event);
    return false;
}

void ClickInstrument::emitSignals(QGraphicsView *view, QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    ClickInfo info(static_cast<ClickInfoPrivate *>(event));

    if(m_isDoubleClick) {
        qDebug() << "DOUBLE CLICK";
        emit doubleClicked(view, item, info);
    } else {
        qDebug() << "CLICK";
        emit clicked(view, item, info);
    }
}

void ClickInstrument::emitSignals(QGraphicsView *view, QGraphicsScene *, QGraphicsSceneMouseEvent *event) {
    ClickInfo info(static_cast<ClickInfoPrivate *>(event));

    if(m_isDoubleClick) {
        emit doubleClicked(view, info);
    } else {
        emit clicked(view, info);
    }
}

void ClickInstrument::startDrag(DragInfo *) {
    dragProcessor()->reset();
}

void ClickInstrument::finishDragProcess(DragInfo *) {
    m_isClick = false;
    m_isDoubleClick = false;
}

bool ClickInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mousePressEventInternal(item, event);
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mouseDoubleClickEventInternal(item, event);
}

bool ClickInstrument::mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mouseMoveEventInternal(item, event);
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mouseReleaseEventInternal(item, event);
}

bool ClickInstrument::mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    return mousePressEventInternal(scene, event);
}

bool ClickInstrument::mouseDoubleClickEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    return mouseDoubleClickEventInternal(scene, event);
}

bool ClickInstrument::mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    return mouseMoveEventInternal(scene, event);
}

bool ClickInstrument::mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) {
    return mouseReleaseEventInternal(scene, event);
}


Qt::MouseButton ClickInfo::button() const {
    return d->button();
}

Qt::MouseButtons ClickInfo::buttons() const {
    return d->buttons();
}

QPointF ClickInfo::scenePos() const {
    return d->scenePos();
}

QPoint ClickInfo::screenPos() const {
    return d->screenPos();
}

Qt::KeyboardModifiers ClickInfo::modifiers() const {
    return d->modifiers();
}

