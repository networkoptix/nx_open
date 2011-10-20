#include "instrumentitemeventdispatcher.h"
#include <cassert>
#include <QMetaType>
#include <QFocusEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneDragDropEvent>
#include <utility/Warnings.h>
#include "instrument.h"

void InstrumentItemEventDispatcher::registerItem(QGraphicsItem *item) {
    assert(item != NULL);

    if(m_instrumentsByItem.contains(item)) {
        qnWarning("Given item is already registered with this instrument item event dispatcher.");
        return;
    }

    QList<Instrument *> &instruments = m_instrumentsByItem[item];
    foreach (Instrument *instrument, m_instruments)
        if (instrument->isWillingToWatch(item))
            instruments.push_back(instrument);
}

void InstrumentItemEventDispatcher::unregisterItem(QGraphicsItem *item) {
    assert(item != NULL);

    int count = m_instrumentsByItem.remove(item);
    if (count == 0) {
        qnWarning("Given item is not registered with this instrument item event dispatcher.");
        return;
    }
}

void InstrumentItemEventDispatcher::installInstrument(Instrument *instrument) {
    assert(instrument != NULL);

    if (m_instruments.contains(instrument)) {
        qnWarning("Instrument '%1' is already registered with this instrument item event dispatcher.", instrument->metaObject()->className());
        return;
    }

    if (instrument->thread() != thread()) {
        qnWarning("Cannot install instrument for an instrument item event dispatcher in a different thread.");
        return;
    }

    if(!instrument->watches(Instrument::ITEM))
        return;

    m_instruments.push_back(instrument);
    for (QHash<QGraphicsItem *, QList<Instrument *> >::iterator pos = m_instrumentsByItem.begin(); pos != m_instrumentsByItem.end(); pos++) {
        QGraphicsItem *item = pos.key();
        QList<Instrument *> &instruments = pos.value();

        if (instrument->isWillingToWatch(item))
            instruments.push_back(instrument);
    }
}

void InstrumentItemEventDispatcher::uninstallInstrument(Instrument *instrument) {
    assert(instrument != NULL);

    if(!instrument->watches(Instrument::ITEM))
        return;

    int index = m_instruments.lastIndexOf(instrument);
    if (index == -1) {
        qnWarning("Instrument '%1' is not registered with this instrument item event dispatcher.", instrument->metaObject()->className());
        return;
    }

    m_instruments.removeAt(index);

    for (QHash<QGraphicsItem *, QList<Instrument *> >::iterator pos = m_instrumentsByItem.begin(); pos != m_instrumentsByItem.end(); pos++) {
        QList<Instrument *> &instruments = pos.value();

        int index = instruments.lastIndexOf(instrument);
        if (index != -1) {
            if (!dispatching()) {
                instruments.removeAt(index);
            } else {
                /* This list may currently be in use by dispatch routine. 
                 * So don't make any structural modifications, just NULL out the
                 * uninstalled instrument. */
                instruments[index] = NULL;
            }
        }
    }
}

bool InstrumentItemEventDispatcher::sceneEventFilter(QGraphicsItem *watched, QEvent *event) {
    QHash<QGraphicsItem *, QList<Instrument *> >::iterator pos = m_instrumentsByItem.find(watched);

    if (pos == m_instrumentsByItem.end()) {
        qnWarning("Received an event from an item that is not registered with this instrument item event dispatcher.");
        return false;
    }

    return base_type::dispatch(this, *pos, watched, event);
}

bool InstrumentItemEventDispatcher::dispatch(Instrument *instrument, QGraphicsItem *watched, QEvent *event) {
    switch (event->type()) {
    case QEvent::GraphicsSceneContextMenu:
        return instrument->contextMenuEvent(watched, static_cast<QGraphicsSceneContextMenuEvent *>(event));
    case QEvent::GraphicsSceneDragEnter:
        return instrument->dragEnterEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDragLeave:
        return instrument->dragLeaveEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDragMove:
        return instrument->dragMoveEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDrop:
        return instrument->dropEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::FocusIn:
        return instrument->focusInEvent(watched, static_cast<QFocusEvent *>(event));
    case QEvent::FocusOut:
        return instrument->focusOutEvent(watched, static_cast<QFocusEvent *>(event));
    case QEvent::GraphicsSceneHoverEnter:
        return instrument->hoverEnterEvent(watched, static_cast<QGraphicsSceneHoverEvent *>(event));
    case QEvent::GraphicsSceneHoverLeave:
        return instrument->hoverLeaveEvent(watched, static_cast<QGraphicsSceneHoverEvent *>(event));
    case QEvent::GraphicsSceneHoverMove:
        return instrument->hoverMoveEvent(watched, static_cast<QGraphicsSceneHoverEvent *>(event));
    case QEvent::InputMethod:
        return instrument->inputMethodEvent(watched, static_cast<QInputMethodEvent *>(event));
    case QEvent::KeyPress:
        return instrument->keyPressEvent(watched, static_cast<QKeyEvent *>(event));
    case QEvent::KeyRelease:
        return instrument->keyReleaseEvent(watched, static_cast<QKeyEvent *>(event));
    case QEvent::GraphicsSceneMouseDoubleClick:
        return instrument->mouseDoubleClickEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMouseMove:
        return instrument->mouseMoveEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMousePress:
        return instrument->mousePressEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMouseRelease:
        return instrument->mouseReleaseEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneWheel:
        return instrument->wheelEvent(watched, static_cast<QGraphicsSceneWheelEvent *>(event));
    default:
        return instrument->sceneEvent(watched, static_cast<QEvent *>(event));
    }
}


