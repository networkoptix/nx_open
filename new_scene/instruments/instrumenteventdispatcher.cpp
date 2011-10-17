#include "instrumenteventdispatcher.h"
#include <cassert>
#include <QMetaObject>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTabletEvent>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QFocusEvent>
#include <QPaintEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QActionEvent>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsSceneHelpEvent>
#include <utility/Warnings.h>
#include <utility/CheckedCast.h>
#include "instrument.h"



AbstractInstrumentEventDispatcher::AbstractInstrumentEventDispatcher(QObject *parent):
    base_type(parent)
{
    m_instrumentsByEventType.reserve(QEvent::User + 1);
    for (int i = 0; i <= QEvent::User; i++)
        m_instrumentsByEventType.push_back(QList<Instrument *>());
}

void AbstractInstrumentEventDispatcher::installInstrument(Instrument *instrument, const QSet<QEvent::Type> &eventTypes) {
    assert(instrument != NULL);

    if (m_eventTypesByInstrument.contains(instrument)) {
        qnWarning("Instrument '%1' is already registered with this instrument event dispatcher.", instrument->metaObject()->className());
        return;
    }

    if (instrument->thread() != thread()) {
        qnWarning("Cannot install instrument for an instrument event dispatcher in a different thread.");
        return;
    }

    QSet<QEvent::Type> &localEventTypes = *m_eventTypesByInstrument.insert(instrument, eventTypes);

    foreach (QEvent::Type type, eventTypes) {
        if (type < 0 || type >= m_instrumentsByEventType.size()) {
            qnWarning("Dispatching event type %1 is not supported.", static_cast<int>(type));
            localEventTypes.remove(type);
            continue;
        }

        m_instrumentsByEventType[type].push_back(instrument);
    }
}

void AbstractInstrumentEventDispatcher::uninstallInstrument(Instrument *instrument) {
    assert(instrument != NULL);

    if (!m_eventTypesByInstrument.contains(instrument)) {
        qnWarning("Instrument '%1' is not registered with this instrument event dispatcher.", instrument->metaObject()->className());
        return;
    }

    foreach (QEvent::Type type, m_eventTypesByInstrument.take(instrument)) {
        QList<Instrument *> &list = m_instrumentsByEventType[type];

        if (!dispatching()) {
            list.removeAt(list.lastIndexOf(instrument));
        } else {
            /* This list is currently in use by dispatch routine. 
            * So don't make any structural modifications, just NULL out the
            * uninstalled instrument. */
            list[list.lastIndexOf(instrument)] = NULL;
        }
    }
}

template<class T>
bool InstrumentEventDispatcher<T>::eventFilter(QObject *watched, QEvent *event) {
    /* This is ensured by Qt. Assert just to feel safe. */
    assert(watched->thread() == thread());

    int type = event->type();
    if (type >= m_instrumentsByEventType.size())
        type = m_instrumentsByEventType.size() - 1; /* Last event type works as a wildcard for all other event types. */

    return base_type::dispatch(this, m_instrumentsByEventType[type], checked_cast<T *>(watched), event);
}

template<class T>
bool InstrumentEventDispatcher<T>::dispatch(Instrument *instrument, T *watched, QEvent *event) {
    switch (event->type()) {
    case QEvent::MouseMove:
        return instrument->mouseMoveEvent(watched, static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonPress:
        return instrument->mousePressEvent(watched, static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonRelease:
        return instrument->mouseReleaseEvent(watched, static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonDblClick:
        return instrument->mouseDoubleClickEvent(watched, static_cast<QMouseEvent *>(event));
    case QEvent::Wheel:
        return instrument->wheelEvent(watched, static_cast<QWheelEvent *>(event));
    case QEvent::TabletMove:
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
        return instrument->tabletEvent(watched, static_cast<QTabletEvent *>(event));
    case QEvent::KeyPress: 
        return instrument->keyPressEvent(watched, static_cast<QKeyEvent *>(event));
    case QEvent::KeyRelease:
        return instrument->keyReleaseEvent(watched, static_cast<QKeyEvent *>(event));
    case QEvent::InputMethod:
        return instrument->inputMethodEvent(watched, static_cast<QInputMethodEvent *>(event));
    case QEvent::FocusIn:
        return instrument->focusInEvent(watched, static_cast<QFocusEvent *>(event));
    case QEvent::FocusOut:
        return instrument->focusOutEvent(watched, static_cast<QFocusEvent *>(event));
    case QEvent::Enter:
        return instrument->enterEvent(watched, static_cast<QEvent *>(event));
    case QEvent::Leave:
        return instrument->leaveEvent(watched, static_cast<QEvent *>(event));
    case QEvent::Paint:
        return instrument->paintEvent(watched, static_cast<QPaintEvent *>(event));
    case QEvent::Move:
        return instrument->moveEvent(watched, static_cast<QMoveEvent *>(event));
    case QEvent::Resize:
        return instrument->resizeEvent(watched, static_cast<QResizeEvent *>(event));
    case QEvent::Close:
        return instrument->closeEvent(watched, static_cast<QCloseEvent *>(event));
    case QEvent::ContextMenu:
        return instrument->contextMenuEvent(watched, static_cast<QContextMenuEvent *>(event));
    case QEvent::Drop:
        return instrument->dropEvent(watched, static_cast<QDropEvent *>(event));
    case QEvent::DragEnter:
        return instrument->dragEnterEvent(watched, static_cast<QDragEnterEvent *>(event));
    case QEvent::DragMove:
        return instrument->dragMoveEvent(watched, static_cast<QDragMoveEvent *>(event));
    case QEvent::DragLeave:
        return instrument->dragLeaveEvent(watched, static_cast<QDragLeaveEvent *>(event));
    case QEvent::Show:
        return instrument->showEvent(watched, static_cast<QShowEvent *>(event));
    case QEvent::Hide:
        return instrument->hideEvent(watched, static_cast<QHideEvent *>(event));
    case QEvent::ToolBarChange:
    case QEvent::ActivationChange:
    case QEvent::EnabledChange:
    case QEvent::FontChange:
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::WindowTitleChange:
    case QEvent::IconTextChange:
    case QEvent::ModifiedChange:
    case QEvent::MouseTrackingChange:
    case QEvent::ParentChange:
    case QEvent::WindowStateChange:
    case QEvent::LocaleChange:
    case QEvent::MacSizeChange:
    case QEvent::ContentsRectChange:
    case QEvent::LanguageChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::KeyboardLayoutChange:
        return instrument->changeEvent(watched, static_cast<QEvent *>(event));
    case QEvent::ActionAdded:
    case QEvent::ActionRemoved:
    case QEvent::ActionChanged:
        return instrument->actionEvent(watched, static_cast<QActionEvent *>(event));
    case QEvent::GraphicsSceneDragEnter:
        return instrument->dragEnterEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDragMove:
        return instrument->dragMoveEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDragLeave:
        return instrument->dragLeaveEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDrop:
        return instrument->dropEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneContextMenu:
        return instrument->contextMenuEvent(watched, static_cast<QGraphicsSceneContextMenuEvent *>(event));
    case QEvent::GraphicsSceneMouseMove:
        return instrument->mouseMoveEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMousePress:
        return instrument->mousePressEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMouseRelease:
        return instrument->mouseReleaseEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMouseDoubleClick:
        return instrument->mouseDoubleClickEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneWheel:
        return instrument->wheelEvent(watched, static_cast<QGraphicsSceneWheelEvent *>(event));
    case QEvent::GraphicsSceneHelp:
        return instrument->helpEvent(watched, static_cast<QGraphicsSceneHelpEvent *>(event));
    default:
        return instrument->event(watched, static_cast<QEvent *>(event));
    }
}

/* Some explicit instantiations. */
template class InstrumentEventDispatcher<QWidget>;
template class InstrumentEventDispatcher<QGraphicsScene>;
template class InstrumentEventDispatcher<QGraphicsView>;


