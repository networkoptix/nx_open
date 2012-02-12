#ifndef QN_SHARED_RESOURCE_POINTER_H
#define QN_SHARED_RESOURCE_POINTER_H

#include <QSharedPointer>

template<class T>
class QnSharedResourcePointer: public QSharedPointer<T> {
    typedef QSharedPointer<T> base_type;

public:
    QnSharedResourcePointer() {}

    explicit QnSharedResourcePointer(T *ptr): base_type(ptr) { initialize(*this); }

    template<class Deleter>
    QnSharedResourcePointer(T *ptr, Deleter d): base_type(ptr, d) { initialize(*this); }

    QnSharedResourcePointer(const QSharedPointer<T> &other): base_type(other) {}

    QnSharedResourcePointer<T> &operator=(const QSharedPointer<T> &other) {
        base_type::operator=(other);
        return *this;
    }

    template<class X>
    QnSharedResourcePointer(const QSharedPointer<X> &other): base_type(other) {}

    template<class X>
    QnSharedResourcePointer<T> &operator=(const QSharedPointer<X> &other) {
        base_type::operator=(other);
        return *this;
    }

    template<class X>
    QnSharedResourcePointer(const QWeakPointer<X> &other): base_type(other) {}

    template<class X>
    QnSharedResourcePointer<T> &operator=(const QWeakPointer<X> &other) { 
        base_type::operator=(other);
        return *this; 
    }

    template<class X>
    QnSharedResourcePointer<X> staticCast() const {
        return qSharedPointerCast<X, T>(*this);
    }

    template<class X>
    QnSharedResourcePointer<X> dynamicCast() const {
        return qSharedPointerDynamicCast<X, T>(*this);
    }

    template<class X>
    QnSharedResourcePointer<X> constCast() const {
        return qSharedPointerConstCast<X, T>(*this);
    }

    template<class X>
    QnSharedResourcePointer<X> objectCast() const {
        return qSharedPointerObjectCast<X, T>(*this);
    }

    template<class X>
    static void initialize(const QSharedPointer<X> &other) {
        if(other)
            other->initWeakPointer(other);
    }
};

#endif // QN_SHARED_RESOURCE_POINTER_H
