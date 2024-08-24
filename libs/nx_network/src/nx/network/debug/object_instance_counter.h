// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/socket_global.h>

#include "object_counters.h"

namespace nx::network::debug {

/**
 * Counts type T instantiations by invoking SocketGlobals::instance().debugCounters().
 * To use it, object of this class should be made a private member. Example:
 * <pre><code>
 * class ObjectOfInterest
 * {
 * public:
 *     ....
 * private:
 *     ....
 *     ObjectInstanceCounter<ObjectOfInterest> m_objectInstanceCounter;
 * };
 * </code></pre>
 *
 * SocketGlobals::instance().debugCounters().toJson() will report number of alive ObjectOfInterest.
 */
template<typename T>
class ObjectInstanceCounter
{
public:
    ObjectInstanceCounter();
    ObjectInstanceCounter(const ObjectInstanceCounter&);
    ObjectInstanceCounter(ObjectInstanceCounter&&);

    ObjectInstanceCounter& operator=(const ObjectInstanceCounter&) = default;
    ObjectInstanceCounter& operator=(ObjectInstanceCounter&&) = default;

    ~ObjectInstanceCounter();
};

template<class T>
ObjectInstanceCounter<T>::ObjectInstanceCounter()
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    SocketGlobals::instance().debugCounters().recordObjectCreation<T>();
}

template<class T>
ObjectInstanceCounter<T>::ObjectInstanceCounter(const ObjectInstanceCounter&)
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    SocketGlobals::instance().debugCounters().recordObjectCreation<T>();
}

// This move constructor may throw, that breaks C++ Core Guidelines F.6 and is very unusual.
// We mark it with explicit `noexcept(false)` and suppress the warning.
template<class T>
ObjectInstanceCounter<T>::ObjectInstanceCounter(ObjectInstanceCounter&&)
    #if defined(_MSC_VER)
        #pragma warning(suppress: 26439)
    #endif
    noexcept(false)
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    SocketGlobals::instance().debugCounters().recordObjectCreation<T>();
}

template<class T>
ObjectInstanceCounter<T>::~ObjectInstanceCounter()
{
    SocketGlobals::instance().debugCounters().recordObjectDestruction<T>();
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

} // namespace nx::network::debug
