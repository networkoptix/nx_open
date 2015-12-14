#ifndef QN_SHARED_RESOURCE_POINTER_H
#define QN_SHARED_RESOURCE_POINTER_H

#include <QtCore/QSharedPointer>

template<class Resource>
class QnSharedResourcePointer: public QSharedPointer<Resource> {
    typedef QSharedPointer<Resource> base_type;

public:
    QnSharedResourcePointer() {}

    explicit QnSharedResourcePointer(Resource *ptr): base_type(ptr) { initialize(*this); }

    template<class Deleter>
    QnSharedResourcePointer(Resource *ptr, Deleter d): base_type(ptr, d) { initialize(*this); }

    // copy 
    QnSharedResourcePointer(const QSharedPointer<Resource> &other): base_type(other) {}

    QnSharedResourcePointer<Resource> &operator=(const QSharedPointer<Resource> &other) {
        base_type::operator=(other);
        return *this;
    }

    template<class OtherResource>
    QnSharedResourcePointer(const QSharedPointer<OtherResource> &other): base_type(other) {}

    template<class OtherResource>
    QnSharedResourcePointer<Resource> &operator=(const QSharedPointer<OtherResource> &other) {
        base_type::operator=(other);
        return *this;
    }

    // move 
    template<typename T>
    struct has_resetWeakPointer {
        template<typename U>
        static std::true_type test(decltype(std::declval<U>.resetWeakPointer()) *dummy=0);

        template<typename U>
        static std::false_type test(...);
        
        enum {value = std::is_same<std::true_type, decltype(test<T>(0))>::value};
    };

    template<typename OtherResource>
    typename std::enable_if<has_resetWeakPointer<OtherResource>::value, 
                            QnSharedResourcePointer<Resource>&>::type &operator=(QSharedPointer<OtherResource> &&other) {
        other->resetWeakPointer();
        base_type::operator=(std::move(other));
        return *this;
    }

    template<typename OtherResource>
    typename std::enable_if<!has_resetWeakPointer<OtherResource>::value, 
                            QnSharedResourcePointer<Resource>&>::type &operator=(QSharedPointer<OtherResource> &&other) {
        base_type::operator=(std::move(other));
        return *this;
    }

    template<typename OtherResource> 
    QnSharedResourcePointer(QSharedPointer<OtherResource> &&other, 
                            typename std::enable_if<has_resetWeakPointer<OtherResource>::value>::type *dummy=0) {
        Q_UNUSED(dummy);
        other->resetWeakPointer();
        base_type(std::move(other));
    }

    template<class OtherResource>
    QnSharedResourcePointer(QSharedPointer<OtherResource> &&other, 
                            typename std::enable_if<!has_resetWeakPointer<OtherResource>::value>::type *dummy=0) {
        Q_UNUSED(dummy);
        base_type(std::move(other));
    }

    template<class OtherResource>
    void reset(OtherResource *resource) {
        base_type::reset(resource);
        initialize(*this);
    }

    void reset() {
        if (!base_type::isNull())
            (*this)->resetWeakPointer();
        base_type::reset();
    }

    template<class OtherResource>
    QnSharedResourcePointer(const QWeakPointer<OtherResource> &other): base_type(other) {}

    template<class OtherResource>
    QnSharedResourcePointer<Resource> &operator=(const QWeakPointer<OtherResource> &other) { 
        base_type::operator=(other);
        return *this; 
    }

    template<class OtherResource>
    QnSharedResourcePointer<OtherResource> staticCast() const {
        return qSharedPointerCast<OtherResource, Resource>(*this);
    }

    template<class OtherResource>
    QnSharedResourcePointer<OtherResource> dynamicCast() const {
        return qSharedPointerDynamicCast<OtherResource, Resource>(*this);
    }

    template<class OtherResource>
    QnSharedResourcePointer<OtherResource> constCast() const {
        return qSharedPointerConstCast<OtherResource, Resource>(*this);
    }

    template<class OtherResource>
    QnSharedResourcePointer<OtherResource> objectCast() const {
        return qSharedPointerObjectCast<OtherResource, Resource>(*this);
    }

    template<class OtherResource>
    static void initialize(const QSharedPointer<OtherResource> &resource) {
        if(resource)
            resource->initWeakPointer(resource);
    }
};

#endif // QN_SHARED_RESOURCE_POINTER_H
