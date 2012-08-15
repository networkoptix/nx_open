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
