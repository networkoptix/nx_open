// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <utility>

#include "mutex.h"

namespace nx::utils {

/**
 * A subset of std::atomic<std::shared_ptr<T>>, which libc++ does not provide: load, store,
 * exchange, compare_exchange_strong and compare_exchange_weak, all sequentially consistent. The
 * mutex covers the pointer copy only, so a value can still be built outside it and installed by
 * compare-and-swap. A pointer dropped by an operation is released after the mutex, so the last
 * reference to it may destroy what it owns without holding the lock.
 *
 * Can be replaced with std::atomic<std::shared_ptr<T>> once every supported standard library
 * implements it (P0718R2, __cpp_lib_atomic_shared_ptr).
 */
template<typename T>
class AtomicSharedPtr
{
public:
    AtomicSharedPtr() = default;
    explicit AtomicSharedPtr(std::shared_ptr<T> value):
        m_value(std::move(value))
    {
    }

    AtomicSharedPtr(const AtomicSharedPtr&) = delete;
    AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

    std::shared_ptr<T> load() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_value;
    }

    void store(std::shared_ptr<T> value) { exchange(std::move(value)); }

    std::shared_ptr<T> exchange(std::shared_ptr<T> value)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_value.swap(value);
        return value;
    }

    /**
     * Installs the desired value if the current one is equivalent to the expected: stores the
     * same pointer and shares ownership with it, as std::atomic requires. Otherwise loads the
     * current value into the expected.
     */
    bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired)
    {
        // Declared before the lock, so the value the expected held is released after it.
        std::shared_ptr<T> previousExpected;

        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_value != expected || m_value.owner_before(expected)
            || expected.owner_before(m_value))
        {
            previousExpected = std::exchange(expected, m_value);
            return false;
        }

        m_value.swap(desired); // The replaced value goes with `desired`, after the lock.
        return true;
    }

    /**
     * Never fails spuriously here, but std::atomic, which can replace this class, may fail even
     * when the current value is the expected one. Call it in a loop that retries on any failure,
     * or call compare_exchange_strong.
     */
    bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired)
    {
        return compare_exchange_strong(expected, std::move(desired));
    }

private:
    mutable nx::Mutex m_mutex;
    std::shared_ptr<T> m_value;
};

} // namespace nx::utils
