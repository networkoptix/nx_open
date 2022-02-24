// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_INSTRUMENTED_H
#define QN_INSTRUMENTED_H

#ifndef Q_MOC_RUN
#include <boost/type_traits/is_base_of.hpp>
#endif

#include <QtWidgets/QGraphicsItem>

#include <utils/common/forward.h>

template<class Base, bool baseIsInstrumented>
class Instrumented;

/**
 * Support class for <tt>Instrumented</tt> template. In most cases there is no
 * need to use this class directly. The only possible use is to
 * <tt>dynamic_cast</tt> to it.
 */
class InstrumentedBase {
private:
    InstrumentedBase(): m_scene(nullptr) {}
    virtual ~InstrumentedBase() {}

    void updateScene(QGraphicsScene *scene, QGraphicsItem *item);

    template<class Base, bool baseIsInstrumented>
    friend class ::Instrumented; /* So that only this class can access our methods. */

private:
    QGraphicsScene *m_scene;
};


/**
 * Base class for items that wish to be automatically registered with their
 * scene's instrument managers.
 */
template<class Base, bool baseIsInstrumented = boost::is_base_of<InstrumentedBase, Base>::value>
class Instrumented: public Base, public InstrumentedBase {
public:
    QN_FORWARD_CONSTRUCTOR(Instrumented, Base, { InstrumentedBase::updateScene(this->scene(), this); });

    virtual ~Instrumented() {
        InstrumentedBase::updateScene(nullptr, this);
    }

protected:
    virtual QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) override {
        if(change == QGraphicsItem::ItemSceneHasChanged)
            InstrumentedBase::updateScene(this->scene(), this);

        return Base::itemChange(change, value);
    }
};


/**
 * Specialization that prevents creation of two instrumented data instances in
 * a single class, even if it was tagged as 'Instrumented' several times
 * (e.g. one of its bases is instrumented).
 */
template<class Base>
class Instrumented<Base, true>: public Base {
public:
    using Base::Base;
};

#endif // QN_INSTRUMENTED_H
