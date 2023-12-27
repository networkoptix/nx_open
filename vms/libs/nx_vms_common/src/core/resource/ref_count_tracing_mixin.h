// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <string>

#include <boost/stacktrace.hpp>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include <nx/utils/debug/allocation_analyzer.h>

class NX_VMS_COMMON_API RefcountTracingMixin
{
public:
    static void setClassNameForTracing(const std::string& className);
    static void generateAllocationReport();

protected:
    void traceRefcountIncrement(const void* pointer) const;
    void traceRefcountDecrement() const;

private:
    static nx::utils::debug::AllocationAnalyzer allocationAnalyzer;
    static nx::Mutex mutex;
    static std::set<const void*> mapData;
    static std::string classNameForTracing;

    static void traceRefcountIncrementInternal(void* pointer, const QObject* object);
    static void traceRefcountDecrementInternal(void* pointer);
};
