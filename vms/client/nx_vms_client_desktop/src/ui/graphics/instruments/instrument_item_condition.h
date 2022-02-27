// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

class QGraphicsItem;

class Instrument;

/**
 * Abstract functor class that is used to conditionally disable instruments for
 * some of the items on the scene.
 */
class InstrumentItemCondition
{
public:
    virtual ~InstrumentItemCondition() {};

    /**
     * \param item                      Item that is about to be registered with the given instrument.
     * \param instrument                Instrument.
     * \returns                         Whether it is OK to register the given item with the given instrument.
     */
    virtual bool operator()(QGraphicsItem *item, Instrument *instrument) const = 0;
};

class InstrumentItemConditionAdaptor: public InstrumentItemCondition
{
public:
    using Func1 = std::function<bool(QGraphicsItem* item)>;

    InstrumentItemConditionAdaptor(Func1 functor):
        m_func1(functor)
    {
    }

    virtual bool operator()(QGraphicsItem* item, Instrument* /*instrument*/) const override
    {
        return m_func1(item);
    }

private:
    Func1 m_func1;
};
