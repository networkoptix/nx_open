#ifndef QN_INSTRUMENT_ITEM_EVENT_DISPATCHER_H
#define QN_INSTRUMENT_ITEM_EVENT_DISPATCHER_H

#include "sceneeventfilter.h"
#include "instrumenteventdispatcher.h"

class Instrument;

/**
 * Instrument item event dispatcher can be installed as an event filter
 * for any number of graphics items. It will then forward filtered events to
 * instruments registered with it. 
 */ 
class InstrumentItemEventDispatcher: public detail::InstrumentEventDispatcherBase, public SceneEventFilter {
    typedef detail::InstrumentEventDispatcherBase base_type;

public:
    InstrumentItemEventDispatcher(QObject *parent = NULL): 
        base_type(parent)
    {}

    /**
     * \param item                     Item to register. Must not be NULL.
     */
    void registerItem(QGraphicsItem *item);

    /**
     * \param item                     Item to unregister. Must not be NULL.
     */
    void unregisterItem(QGraphicsItem *item);

    /**
     * \param instrument               Instrument to install. Must not be NULL.
     */
    void installInstrument(Instrument *instrument);

    /**
     * \param instrument               Instrument to uninstall. Must not be NULL.
     */
    void uninstallInstrument(Instrument *instrument);

    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) override;

protected:
    friend class detail::InstrumentEventDispatcherBase; /* Calls dispatch. */

    bool dispatch(Instrument *instrument, QGraphicsItem *watched, QEvent *event);

private:
    /** Item to instruments mapping. */
    QHash<QGraphicsItem *, QList<Instrument *> > m_instrumentsByItem;

    /** List of all installed instruments, in installation order. */
    QList<Instrument *> m_instruments;
};



#endif // QN_INSTRUMENT_ITEM_EVENT_DISPATCHER_H
