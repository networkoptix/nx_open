#include "instrument_event_dispatcher.h"
#include <cassert>
#include <QMetaObject>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include "instrument.h"

template<class T>
bool InstrumentEventDispatcherBase<T>::eventFilter(QObject *target, QEvent *event) {
    /* This is ensured by Qt. Assert just to feel safe. */
    assert(target->thread() == thread());

    return static_cast<InstrumentEventDispatcher<T> *>(this)->dispatch(checked_cast<T *>(target), event);
}

bool InstrumentEventDispatcherBase<QGraphicsItem>::sceneEventFilter(QGraphicsItem *target, QEvent *event) {
    return static_cast<InstrumentEventDispatcher<QGraphicsItem> *>(this)->dispatch(target, event);
}

template<class T>
InstrumentEventDispatcher<T>::InstrumentEventDispatcher(QObject *parent):
    base_type(parent),
    m_dispatchDepth(0)
{}

template<class T>
void InstrumentEventDispatcher<T>::installInstrumentInternal(Instrument *instrument, T *target, InstallationMode::Mode mode, Instrument *reference) {
    if(!registeredNotify(instrument, target))
        return;

    m_instrumentTargets.insert(qMakePair(instrument, target));

    foreach(QEvent::Type eventType, instrument->watchedEventTypes(TARGET_TYPE))
        insertInstrument(instrument, mode, reference, &m_instrumentsByTarget[qMakePair(target, eventType)]);
}

template<class T>
void InstrumentEventDispatcher<T>::uninstallInstrumentInternal(Instrument *instrument, T *target) {
    typename QSet<QPair<Instrument *, T*> >::iterator pos = m_instrumentTargets.find(qMakePair(instrument, target));
    if(pos == m_instrumentTargets.end())
        return;

    unregisteredNotify(instrument, target);

    m_instrumentTargets.erase(pos);

    foreach(QEvent::Type eventType, instrument->watchedEventTypes(TARGET_TYPE)) {
        QList<Instrument *> &list = m_instrumentsByTarget[qMakePair(target, eventType)];

        if(!dispatching()) {
            list.removeOne(instrument);
        } else {
            /* This list may currently be in use by dispatch routine. 
             * So don't make any structural modifications, just NULL out the
             * uninstalled instrument. */
            list[list.indexOf(instrument)] = NULL;
        }
    }
}

template<class T>
void InstrumentEventDispatcher<T>::installInstrument(Instrument *instrument, InstallationMode::Mode mode, Instrument *reference) {
    assert(instrument != NULL);

    if (m_instruments.contains(instrument)) {
        qnWarning("Instrument '%1' is already registered with this instrument event dispatcher.", instrument->metaObject()->className());
        return;
    }

    if (instrument->thread() != this->thread()) {
        qnWarning("Cannot install instrument for an instrument event dispatcher in a different thread.");
        return;
    }

    insertInstrument(instrument, mode, reference, &m_instruments);

    if(instrument->watches(TARGET_TYPE))
        foreach(T *target, m_targets)
            installInstrumentInternal(instrument, target, mode, reference);
}

template<class T>
void InstrumentEventDispatcher<T>::uninstallInstrument(Instrument *instrument) {
    assert(instrument != NULL);

    if (!m_instruments.contains(instrument)) {
        qnWarning("Instrument '%1' is not registered with this instrument event dispatcher.", instrument->metaObject()->className());
        return;
    }

    m_instruments.removeOne(instrument);

    if(instrument->watches(TARGET_TYPE))
        foreach(T *target, m_targets)
            uninstallInstrumentInternal(instrument, target);
}

template<class T>
void InstrumentEventDispatcher<T>::registerTarget(T *target) {
    assert(target != NULL);

    if(m_targets.contains(target)) {
        qnWarning("Given target is already registered with this instrument event dispatcher.");
        return;
    }

    m_targets.insert(target);

    foreach(Instrument *instrument, m_instruments)
        installInstrumentInternal(instrument, target, InstallFirst, NULL);
}

template<class T>
void InstrumentEventDispatcher<T>::unregisterTarget(T *target) {
    assert(target != NULL);

    if(!m_targets.contains(target)) {
        qnWarning("Given target is not registered with this instrument event dispatcher.");
        return;
    }

    m_targets.remove(target);

    foreach(Instrument *instrument, m_instruments) 
        uninstallInstrumentInternal(instrument, target);
}

template<class T>
bool InstrumentEventDispatcher<T>::dispatch(T *target, QEvent *event) {
    typename QHash<QPair<T *, QEvent::Type>, QList<Instrument *> >::iterator pos = m_instrumentsByTarget.find(qMakePair(target, event->type()));
    if (pos == m_instrumentsByTarget.end()) {
#ifdef _DEBUG
        if(!m_targets.contains(target))
            qnWarning("Received an event from a target that is not registered with this instrument event dispatcher.");
#endif
        return false;
    }

    QList<Instrument *> &instruments = *pos;
    if (instruments.empty())
        return false;

    bool result = false;
    bool hasNullInstruments = false;

    m_dispatchDepth++;

    /* Instruments may be added during iteration, so we cannot use iterators. */
    for (int i = instruments.size() - 1; i >= 0; i--) {
        Instrument *instrument = instruments[i];

        if (instrument == NULL) {
            hasNullInstruments = true;
            continue;
        }

        if (!instrument->isEnabled())
            continue;

        if (dispatch(instrument, target, event)) {
            result = true;
            break;
        }
    }

    m_dispatchDepth--;

    if (hasNullInstruments && m_dispatchDepth == 0)
        instruments.removeAll(NULL);

    return result;
}

/* Some explicit instantiations. */
template class InstrumentEventDispatcher<QGraphicsScene>;
template class InstrumentEventDispatcher<QGraphicsView>;
template class InstrumentEventDispatcher<QWidget>;
template class InstrumentEventDispatcher<QGraphicsItem>;


