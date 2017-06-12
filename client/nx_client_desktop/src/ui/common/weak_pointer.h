#ifndef QN_WEAK_POINTER_H
#define QN_WEAK_POINTER_H

#include <QtCore/QWeakPointer>
#include "weak_graphics_item_pointer.h"

/**
 * Weak pointer class that also supports <tt>QGraphicsItem</tt>s.
 */ 
template<class T>
class WeakPointer: public QPointer<T> {
    typedef QPointer<T> base_type;

public:
    WeakPointer(): base_type() {}
    WeakPointer(QWeakPointer<T> &other): base_type(other) {}
    WeakPointer(const QSharedPointer<T> &other): base_type(other) {}

    WeakPointer<T> &operator=(const QPointer<T> &other) {
        base_type::operator=(other);
        return *this;
    }

    WeakPointer<T> &operator=(const QSharedPointer<T> &other) {
        base_type::operator=(other);
        return *this;
    }

    template<class Y>
    WeakPointer(Y *other): base_type(other) {}

    template<class Y>
    WeakPointer<T> &operator=(Y *other) {
        base_type::operator=(other);
        return *this;
    }
};


template<>
class WeakPointer<QGraphicsItem>: public WeakGraphicsItemPointer {
    typedef WeakGraphicsItemPointer base_type;
public:
    WeakPointer(): base_type() {}

    WeakPointer(const WeakGraphicsItemPointer &other): base_type(other) {}

    WeakPointer(QGraphicsItem *item): base_type(item) {}

    WeakPointer<QGraphicsItem> &operator=(const WeakGraphicsItemPointer &other) {
        base_type::operator=(other);
        return *this;
    }

    WeakPointer<QGraphicsItem> &operator=(QGraphicsItem *item) {
        base_type::operator=(item);
        return *this;
    }
};

#endif // QN_WEAK_POINTER_H
