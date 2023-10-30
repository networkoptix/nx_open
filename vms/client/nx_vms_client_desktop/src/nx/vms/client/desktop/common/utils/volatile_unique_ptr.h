// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

namespace nx::vms::client::desktop {

/**
 * This is an unique pointer to a QObject that can be destroyed elsewhere.
 * While this semantic is not the best in the sense of clear memory management,
 * it's quite convenient for transient objects like animations or single-shot timers
 * if we need to track them and probably delete early.
 */
template<typename T, typename Deleter = QScopedPointerDeleter<T>>
class VolatileUniquePtr
{
public:
    VolatileUniquePtr() = default;
    VolatileUniquePtr(VolatileUniquePtr&) = delete;
    VolatileUniquePtr(VolatileUniquePtr&&) = default;
    VolatileUniquePtr& operator=(VolatileUniquePtr&) = delete;
    VolatileUniquePtr& operator=(VolatileUniquePtr&&) = default;

    VolatileUniquePtr(T* raw): m_ptr(raw) {}
    ~VolatileUniquePtr() { reset(); }

    T* get() const { return m_ptr; }
    T* data() const { return get(); }
    T* operator->() const { return get(); }

    explicit operator bool() const { return !m_ptr.isNull(); }

    T* release()
    {
        QPointer<T> result;
        result.swap(m_ptr);
        return result;
    }

    void reset(T* raw = nullptr)
    {
        auto old = m_ptr;
        m_ptr = raw;
        Deleter::cleanup(old);
    }

    void swap(VolatileUniquePtr& other)
    {
        m_ptr.swap(other.m_ptr);
    }

private:
    QPointer<T> m_ptr;
};

} // namespace nx::vms::client::desktop
