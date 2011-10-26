#include "instrumentitemeventdispatcher.h"
#include <cassert>
#include <QMetaType>
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
    return instrument->sceneEvent(watched, event);
}


