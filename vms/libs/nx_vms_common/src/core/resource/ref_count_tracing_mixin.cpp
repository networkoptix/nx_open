// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifdef TRACE_REFCOUNT_CHANGE

#include "ref_count_tracing_mixin.h"

#include <cstring>

nx::utils::debug::AllocationAnalyzer RefcountTracingMixin::allocationAnalyzer;
nx::Mutex RefcountTracingMixin::mutex;
std::set<const void*> RefcountTracingMixin::mapData;
std::string RefcountTracingMixin::classNameForTracing;

void RefcountTracingMixin::setClassNameForTracing(const std::string& className)
{
    NX_MUTEX_LOCKER lock(&mutex);
    classNameForTracing = className;
}

void RefcountTracingMixin::generateAllocationReport()
{
    NX_INFO(NX_SCOPE_TAG, "Non-freed objects:");
    NX_INFO(NX_SCOPE_TAG, allocationAnalyzer.generateReport());
}

void RefcountTracingMixin::traceRefcountIncrement(const void* pointer) const
{
    // pointer must point to an instance of a class derived from QObject, otherwise a crash is
    // expected. reinterpret_cast is needed because of compilation problems due to incomplete
    // types.
    traceRefcountIncrementInternal((void*)this, reinterpret_cast<const QObject*>(pointer));
}

void RefcountTracingMixin::traceRefcountDecrement() const
{
    traceRefcountDecrementInternal((void*)this);
}

void RefcountTracingMixin::traceRefcountIncrementInternal(void* pointer, const QObject* object)
{
    if (!object || !object->metaObject())
        return;

    const char* dataName = object->metaObject()->className();

    NX_MUTEX_LOCKER lock(&mutex);

    if (!classNameForTracing.empty() && !strstr(dataName, classNameForTracing.c_str()))
        return;

    if (mapData.insert(pointer).second)
    {
        allocationAnalyzer.recordObjectCreation(pointer);
    }
}

void RefcountTracingMixin::traceRefcountDecrementInternal(void* pointer)
{
    NX_MUTEX_LOCKER lock(&mutex);

    if (auto it = mapData.find(pointer); it != mapData.end())
    {
        mapData.erase(it);
        allocationAnalyzer.recordObjectDestruction(pointer);
    }
}

#endif // TRACE_REFCOUNT_CHANGE
