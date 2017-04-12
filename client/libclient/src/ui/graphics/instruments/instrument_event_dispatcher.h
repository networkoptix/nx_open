#ifndef QN_INSTRUMENT_EVENT_DISPATCHER_H
#define QN_INSTRUMENT_EVENT_DISPATCHER_H

#include <typeinfo>

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QEvent> /* For QEvent::Type. */
#include <QtCore/QPair>

#include "instrument.h"
#include "scene_event_filter.h"
#include "installation_mode.h"

template<class T>
class InstrumentEventDispatcherBase: public QObject {
public:
    InstrumentEventDispatcherBase(QObject *parent = NULL): QObject(parent) {}

    virtual bool eventFilter(QObject *target, QEvent *event) override;
};

template<>
class InstrumentEventDispatcherBase<QGraphicsItem>: public QObject, public SceneEventFilter {
public:
    InstrumentEventDispatcherBase(QObject *parent = NULL): QObject(parent) {}

    virtual bool sceneEventFilter(QGraphicsItem *target, QEvent *event) override;
};


/**
 * Instrument event dispatcher can be installed as an event filter for any
 * number of objects of the given type T. It will then forward filtered
 * events to instruments registered with it.
 *
 * Note that it is possible to install an event dispatcher as an event
 * filter for object of any type, not only of T. Type mismatches are checked
 * only in debug mode, so the user must pay attention.
 *
 * \tparam T                         Type that watched objects are cast to
 *                                   before being passed to instruments.
 */
template<class T>
class InstrumentEventDispatcher: public InstrumentEventDispatcherBase<T>, public InstallationMode {
    typedef InstrumentEventDispatcherBase<T> base_type;

    friend class InstrumentEventDispatcherBase<T>; /* Calls dispatch. */

public:
    static const Instrument::WatchedType TARGET_TYPE = instrument_watched_type<T>::value;

    /**
     * Constructor.
     *
     * \param parent                   Parent object for this instrument event dispatcher.
     */
    InstrumentEventDispatcher(QObject *parent = NULL);

    /**
     * \param instrument               Instrument to install. Must not be NULL.
     */
    void installInstrument(Instrument *instrument, InstallationMode::Mode mode, Instrument *reference);

    /**
     * \param instrument               Instrument to uninstall. Must not be NULL.
     */
    void uninstallInstrument(Instrument *instrument);

    /**
     * \param target                   Target to register. Must not be NULL.
     */
    void registerTarget(T *target);

    /**
     * \param target                   Target to unregister. Must not be NULL.
     */
    void unregisterTarget(T *target);

protected:
    bool dispatching() const {
        return m_dispatchDepth > 0;
    }

    bool dispatch(T *target, QEvent *event);

private:
    void installInstrumentInternal(Instrument *instrument, T *target, InstallationMode::Mode mode, Instrument *reference);

    void uninstallInstrumentInternal(Instrument *instrument, T *target);

    template<class Y>
    bool dispatch(Instrument *instrument, Y *target, QEvent *event) const {
        return instrument->event(target, event);
    }

    bool dispatch(Instrument *instrument, QGraphicsItem *target, QEvent *event) const {
        return instrument->sceneEvent(target, event);
    }

    bool registeredNotify(Instrument *, QGraphicsScene *) const {
        return true;
    }

    bool registeredNotify(Instrument *instrument, QGraphicsItem *target) const;

    template<class Y>
    bool registeredNotify(Instrument *instrument, Y *target) const {
        return instrument->registeredNotify(target);
    }

    void unregisteredNotify(Instrument *, QGraphicsScene *) const {
        return;
    }

    template<class Y>
    void unregisteredNotify(Instrument *instrument, Y *target) const {
        return instrument->unregisteredNotify(target);
    }

protected:
    struct TargetData {
        TargetData() {}
        TargetData(const std::type_info *typeInfo): typeInfo(typeInfo) {}

        /** Runtime type of the target. */
        const std::type_info *typeInfo;

        /** Installed instruments. */
        QList<Instrument *> instruments;
    };

    /** Current dispatch depth. */
    int m_dispatchDepth;

    /** Mapping from target to its runtime type. */
    QHash<T *, const std::type_info *> m_typeInfoByTarget;

    /** List of all installed instruments, in installation order. */
    QList<Instrument *> m_instruments;

    /** Set of all functional instrument-target pairs. */
    QSet<QPair<Instrument *, T*> > m_instrumentTargets;

    /** Main dispatch structure. Mapping from target-event pair to list of associated instruments. */
    QHash<QPair<T *, QEvent::Type>, TargetData> m_dataByTarget;
};

#endif // QN_INSTRUMENT_EVENT_DISPATCHER_H
