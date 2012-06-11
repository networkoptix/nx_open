#ifndef QN_INSTRUMENT_ITEM_CONDITION_H
#define QN_INSTRUMENT_ITEM_CONDITION_H

#include <functional>

class QGraphicsItem;

class Instrument;

/**
 * Abstract functor class that is used to conditionally disable instruments for
 * some of the items on the scene.
 */
class InstrumentItemCondition: public std::binary_function<QGraphicsItem *, Instrument *, bool> {
public:
    /**
     * \param item                      Item that is about to be registered with the given instrument.
     * \param instrument                Instrument.
     * \returns                         Whether it is OK to register the given item with the given instrument.
     */
    virtual bool operator()(QGraphicsItem *item, Instrument *instrument) const = 0;
};

#endif // QN_INSTRUMENT_ITEM_CONDITION_H
