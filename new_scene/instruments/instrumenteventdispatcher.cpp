#include "instrumenteventdispatcher.h"
#include <cassert>
#include <QMetaObject>
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
    return instrument->event(watched, event);
}

/* Some explicit instantiations. */
template class InstrumentEventDispatcher<QWidget>;
template class InstrumentEventDispatcher<QGraphicsScene>;
template class InstrumentEventDispatcher<QGraphicsView>;


