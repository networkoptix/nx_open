// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>
#include <QtCore/QObject>

#include <nx/utils/log/to_string.h>

#ifdef TRACE_REFCOUNT_CHANGE
    #include "ref_count_tracing_mixin.h"
#endif

/**
 * Shared pointer implementation via QSharedPointer, adding a conversion from `this` to "shared".
 * See the QnFromThisToShared helper class.
 */
template<class Resource>
class QnSharedResourcePointer: public QSharedPointer<Resource>
    #ifdef TRACE_REFCOUNT_CHANGE
        , public RefcountTracingMixin
    #endif
{
    typedef QSharedPointer<Resource> base_type;

public:
    QnSharedResourcePointer()
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
    }

   ~QnSharedResourcePointer()
   {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountDecrement();
        #endif
   }

    explicit QnSharedResourcePointer(Resource* ptr):
        base_type(ptr)
    {
        initialize(*this);
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
    }

    template<class Deleter>
    QnSharedResourcePointer(Resource* ptr, Deleter d):
        base_type(ptr, d)
    {
        initialize(*this);
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
    }

    // copy
    QnSharedResourcePointer(const QnSharedResourcePointer<Resource>& other):
        base_type(other)
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
    }

    QnSharedResourcePointer(const QSharedPointer<Resource>& other):
        base_type(other)
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
    }

    QnSharedResourcePointer<Resource>& operator=(const QnSharedResourcePointer<Resource>& other)
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountDecrement();
        #endif
        base_type::operator=(other);
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
        return *this;
    }

    template<class OtherResource>
    QnSharedResourcePointer(const QnSharedResourcePointer<OtherResource>& other):
        base_type(other)
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
    }

    template<class OtherResource>
    QnSharedResourcePointer<Resource>& operator=(const QnSharedResourcePointer<OtherResource>& other)
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountDecrement();
        #endif
        base_type::operator=(other);
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
        return *this;
    }

    // move
    QnSharedResourcePointer(QnSharedResourcePointer<Resource>&& other):
        base_type(std::move(other))
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
    }

    QnSharedResourcePointer<Resource>& operator=(QnSharedResourcePointer<Resource>&& other)
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountDecrement();
        #endif
        base_type::operator=(std::move(other));
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
        return *this;
    }

    template<class OtherResource>
    QnSharedResourcePointer(QnSharedResourcePointer<OtherResource>&& other):
        base_type(std::move(other))
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
    }

    template<class OtherResource>
    QnSharedResourcePointer<Resource>& operator=(QnSharedResourcePointer<OtherResource>&& other)
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountDecrement();
        #endif
        base_type::operator=(std::move(other));
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
        return *this;
    }

    void reset()
    {
        base_type::reset();
    }

    template<class OtherResource>
    void reset(OtherResource *resource)
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountDecrement();
        #endif
        base_type::reset(resource);
        initialize(*this);
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
    }

    template<class OtherResource>
    QnSharedResourcePointer(const QWeakPointer<OtherResource>& other):
        base_type(other)
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
    }

    template<class OtherResource>
    QnSharedResourcePointer<Resource>& operator=(const QWeakPointer<OtherResource>& other)
    {
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountDecrement();
        #endif
        base_type::operator=(other);
        #ifdef TRACE_REFCOUNT_CHANGE
            RefcountTracingMixin::traceRefcountIncrement(this->data());
        #endif
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
    static void initialize(const QnSharedResourcePointer<OtherResource>& resource)
    {
        if (resource)
            resource->initWeakPointer(resource);
    }

    QString toString() const
    {
        return nx::toString(this->data());
    }
};
