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
    virtual ~InstrumentItemCondition() {};

    /**
     * \param item                      Item that is about to be registered with the given instrument.
     * \param instrument                Instrument.
     * \returns                         Whether it is OK to register the given item with the given instrument.
     */
    virtual bool operator()(QGraphicsItem *item, Instrument *instrument) const = 0;
};


/**
 * Instrument item condition that adapts the given functor.
 * 
 * \tparam Functor                      Functor to adapt.
 * \tparam argc                         Number of arguments that the functor accepts.
 *                                      Valid values are 1 and 2.
 */
template<class Functor, int argc = 1>
class InstrumentItemConditionAdaptor: public InstrumentItemCondition {
public:
    InstrumentItemConditionAdaptor(const Functor &functor = Functor()): m_functor(functor) {}

    virtual bool operator()(QGraphicsItem *item, Instrument *instrument) const override {
        return invoke(item, instrument, Dummy<argc>());
    }

private:
    template<int> struct Dummy {};

    bool invoke(QGraphicsItem *item, Instrument *, const Dummy<1> &) const {
        return m_functor(item);
    }

    bool invoke(QGraphicsItem *item, Instrument *instrument, const Dummy<2> &) const {
        return m_functor(item, instrument);
    }

private:
    Functor m_functor;
};


#endif // QN_INSTRUMENT_ITEM_CONDITION_H
