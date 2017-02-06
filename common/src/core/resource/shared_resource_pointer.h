#pragma once

#include <QtCore/QSharedPointer>

template<class Resource>
class QnSharedResourcePointer: public QSharedPointer<Resource>
{
    typedef QSharedPointer<Resource> base_type;

public:
    QnSharedResourcePointer() {}

    explicit QnSharedResourcePointer(Resource* ptr):
        base_type(ptr)
    {
        initialize(*this);
    }

    template<class Deleter>
    QnSharedResourcePointer(Resource* ptr, Deleter d):
        base_type(ptr, d)
    {
        initialize(*this);
    }

    // copy
    QnSharedResourcePointer(const QSharedPointer<Resource>& other):
        base_type(other)
    {
    }

    QnSharedResourcePointer<Resource>& operator=(const QSharedPointer<Resource>& other)
    {
        base_type::operator=(other);
        return *this;
    }

    template<class OtherResource>
    QnSharedResourcePointer(const QSharedPointer<OtherResource>& other):
        base_type(other)
    {
    }

    template<class OtherResource>
    QnSharedResourcePointer<Resource>& operator=(const QSharedPointer<OtherResource>& other)
    {
        base_type::operator=(other);
        return *this;
    }

    // move
    QnSharedResourcePointer(QSharedPointer<Resource>&& other):
        base_type(std::move(other))
    {
        other.clear();
    }

    QnSharedResourcePointer<Resource>& operator=(QSharedPointer<Resource>&& other)
    {
        base_type::operator=(std::move(other));
        other.clear();
        return *this;
    }

    template<class OtherResource>
    QnSharedResourcePointer(QSharedPointer<OtherResource>&& other):
        base_type(std::move(other))
    {
        other.clear();
    }

    template<class OtherResource>
    QnSharedResourcePointer<Resource>& operator=(QSharedPointer<OtherResource>&& other)
    {
        base_type::operator=(std::move(other));
        other.reset();
        return *this;
    }

    void reset()
    {
        base_type::reset();
    }

    template<class OtherResource>
    void reset(OtherResource *resource)
    {
        base_type::reset(resource);
        initialize(*this);
    }

    template<class OtherResource>
    QnSharedResourcePointer(const QWeakPointer<OtherResource>& other):
        base_type(other)
    {
    }

    template<class OtherResource>
    QnSharedResourcePointer<Resource>& operator=(const QWeakPointer<OtherResource>& other)
    {
        base_type::operator=(other);
        return *this;
    }

    template<class OtherResource>
    QnSharedResourcePointer<OtherResource> staticCast() const
    {
        return qSharedPointerCast<OtherResource, Resource>(*this);
    }

    template<class OtherResource>
    QnSharedResourcePointer<OtherResource> dynamicCast() const
    {
        return qSharedPointerDynamicCast<OtherResource, Resource>(*this);
    }

    template<class OtherResource>
    QnSharedResourcePointer<OtherResource> constCast() const
    {
        return qSharedPointerConstCast<OtherResource, Resource>(*this);
    }

    template<class OtherResource>
    QnSharedResourcePointer<OtherResource> objectCast() const
    {
        return qSharedPointerObjectCast<OtherResource, Resource>(*this);
    }

    template<class OtherResource>
    static void initialize(const QSharedPointer<OtherResource>& resource)
    {
        if (resource)
            resource->initWeakPointer(resource);
    }
};
