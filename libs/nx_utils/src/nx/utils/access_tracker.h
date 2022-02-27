// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <utility>

namespace nx::utils {

namespace detail {

template<typename T>
class AccessTrackerBase
{
public:
    AccessTrackerBase() = delete;

    template<typename... Args>
    constexpr AccessTrackerBase(std::in_place_t, Args&&... args):
        m_val(std::forward<Args>(args)...)
    {
    }

    constexpr AccessTrackerBase(T&& val):
        m_val(std::move(val))
    {
    }

    constexpr AccessTrackerBase(const T& val):
        m_val(val)
    {
    }

    const T& operator*() const&
    {
        m_accessed = true;
        return m_val;
    }

    T& operator*()&
    {
        m_accessed = true;
        return m_val;
    }

    const T& get() const
    {
        m_accessed = true;
        return m_val;
    }

    T& get()
    {
        m_accessed = true;
        return m_val;
    }

    /**
     * Constructs stored value inplace.s
     */
    template<typename... Args>
    void emplace(Args&&... args)
    {
        std::destroy_at(&m_val);
        // TODO: Invoke std::construct_at in c++20.
        new(&m_val) T(std::forward<Args>(args)...);
        m_accessed = false;
    }

    bool wasAccessed() const
    {
        return m_accessed;
    }

protected:
    T m_val;
    mutable bool m_accessed = false;
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

/**
 * Wraps a value and remembers if it was accessed.
 * Example:
 * <pre><code>
 * AccessTracker<std::string> str(std::in_place, "Hello, world");
 * assert(!str.wasAccessed());
 * str->append("!");
 * assert(str.wasAccessed());
 * </code></pre>
 */
template<typename T>
class AccessTracker:
    public detail::AccessTrackerBase<T>
{
    using base_type = detail::AccessTrackerBase<T>;

public:
    using base_type::base_type;

    const T* operator->() const
    {
        this->m_accessed = true;
        return &this->m_val;
    }

    T* operator->()
    {
        this->m_accessed = true;
        return &this->m_val;
    }

    AccessTracker& operator=(const T& val)
    {
        this->m_val = val;
        this->m_accessed = false;
        return *this;
    }

    AccessTracker& operator=(T&& val)
    {
        this->m_val = std::move(val);
        this->m_accessed = false;
        return *this;
    }
};

template<typename T>
class AccessTracker<T*>:
    public detail::AccessTrackerBase<T*>
{
    using base_type = detail::AccessTrackerBase<T*>;

public:
    using base_type::base_type;

    const T* operator->() const
    {
        this->m_accessed = true;
        return this->m_val;
    }

    T* operator->()
    {
        this->m_accessed = true;
        return this->m_val;
    }

    AccessTracker& operator=(T* val)
    {
        this->m_val = val;
        this->m_accessed = false;
        return *this;
    }
};

} // namespace nx::utils
