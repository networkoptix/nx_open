#ifndef QN_INSTRUMENT_EVENT_DISPATCHER_H
#define QN_INSTRUMENT_EVENT_DISPATCHER_H

#include <QObject>
#include <QSet>
#include <QList>
#include <QHash>
#include <QEvent> /* For QEvent::Type. */

class Instrument;

namespace detail {
    class InstrumentEventDispatcherBase: public QObject {
    public:
        InstrumentEventDispatcherBase(QObject *parent = NULL):
            QObject(parent),
            m_dispatchDepth(0)
        {}

    protected:
        template<class Derived, class T>
        bool dispatch(Derived *derived, QList<Instrument *> &instruments, T *watched, QEvent *event) {
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

                if (derived->dispatch(instrument, watched, event)) {
                    result = true;
                    break;
                }
            }

            m_dispatchDepth--;

            if (hasNullInstruments && m_dispatchDepth == 0)
                instruments.removeAll(NULL);

            return result;
        }

        bool dispatching() const {
            return m_dispatchDepth > 0;
        }

    private:
        /** Current dispatch depth. */
        int m_dispatchDepth;
    };

} // namespace detail


/**
 * Base class for instrument event dispatchers.
 */
class AbstractInstrumentEventDispatcher: public detail::InstrumentEventDispatcherBase {
    typedef detail::InstrumentEventDispatcherBase base_type;

public:
    /**
     * Constructor.
     * 
     * \param parent                   Parent object for this instrument event dispatcher.
     */
    AbstractInstrumentEventDispatcher(QObject *parent = NULL);

    /**
     * Installs given instrument to filter the given set of given event types. 
     * 
     * Note that <tt>QEvent::User</tt> acts as a wildcard for all user types.
     * Also note that this function cannot be used several times on the same 
     * instrument, even if the provided event types are different.
     * 
     * \param instrument               Instrument to install. Must not be NULL.
     * \param eventTypes               Set of event types that the given instrument will filter.
     */
    void installInstrument(Instrument *instrument, const QSet<QEvent::Type> &eventTypes);

    /**
     * \param instrument               Instrument to uninstall. Must not be NULL.
     */
    void uninstallInstrument(Instrument *instrument);

protected:
    /**
     * This function is here to make this class abstract, as its name says.
     */
    virtual void abstractFunction() = 0;

protected:
    /** Event type to instrument list mapping. */
    QList<QList<Instrument *> > m_instrumentsByEventType;

    /** Instrument to event types mapping. */
    QHash<Instrument *, QSet<QEvent::Type> > m_eventTypesByInstrument;
};


/**
 * Instrument event dispatcher can be installed as an event filter for any
 * number of objects of the given type T. It will then forward filtered 
 * events to instruments registered with it.
 *
 * Note that it is possible to install this event dispatcher as an event 
 * filter for object of any type, not only of T. Type mismatches are checked
 * only in debug mode, so the user must pay attention.
 *
 * \tparam T                         Type that watched objects are cast to
 *                                   before being passed to instruments.
 */
template<class T>
class InstrumentEventDispatcher: public AbstractInstrumentEventDispatcher {
    typedef AbstractInstrumentEventDispatcher base_type;

public:
    InstrumentEventDispatcher(QObject *parent = NULL): 
        AbstractInstrumentEventDispatcher(parent)
    {}

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

protected:
    friend class detail::InstrumentEventDispatcherBase; /* Calls dispatch. */

    bool dispatch(Instrument *instrument, T *watched, QEvent *event);

private:
    virtual void abstractFunction() override {};
};

#endif // QN_INSTRUMENT_EVENT_DISPATCHER_H
