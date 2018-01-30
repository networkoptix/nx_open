#pragma once

#include <atomic>
#include <memory>

namespace nx {
namespace utils {

/**
 * Same as std::unique_ptr, but all operations with internal pointer are done atomically.
 */
template<typename T>
class AtomicUniquePtr
{
    typedef void (AtomicUniquePtr<T>::*bool_type)() const;
    void this_type_does_not_support_comparisons() const {}

public:
    AtomicUniquePtr(T* ptr = nullptr)
    :
        m_ptr(ptr)
    {
    }
    template<class D>
    AtomicUniquePtr(std::unique_ptr<D> ptr)
    :
        m_ptr(ptr.release())
    {
    }
    AtomicUniquePtr(AtomicUniquePtr&& rhs)
    {
        auto ptr = rhs.m_ptr.exchange(nullptr);
        m_ptr.store(ptr);
    }
    ~AtomicUniquePtr()
    {
        auto ptr = m_ptr.exchange(nullptr);
        delete ptr;
    }

    void reset(T* ptr = nullptr)
    {
        auto oldPtr = m_ptr.exchange(ptr);
        delete oldPtr;
    }
    T* release()
    {
        return m_ptr.exchange(nullptr);
    }
    T* get() const
    {
        return m_ptr.load();
    }

    template<typename D>
    AtomicUniquePtr& operator=(AtomicUniquePtr<D>&& rhs)
    {
        if (this == &rhs)
            return *this;

        reset(rhs.release());
        return *this;
    }
    template<typename D>
    AtomicUniquePtr& operator=(std::unique_ptr<D>&& rhs)
    {
        reset(rhs.release());
        return *this;
    }
    T* operator->() const
    {
        return get();
    }

    operator bool_type() const
    {
        return m_ptr.load()
            ? &AtomicUniquePtr<T>::this_type_does_not_support_comparisons
            : nullptr;
    }

private:
    std::atomic<T*> m_ptr;
};

template<typename T, typename ... Param>
AtomicUniquePtr<T> make_atomic_unique(Param&& ... params)
{
    return AtomicUniquePtr<T>(new T(std::forward<Param>(params)...));
}

}   //utils
}   //nx

namespace std {

template <typename T>
struct DependentFalse
{
    static constexpr bool value = false;
};

template<typename T>
void swap(nx::utils::AtomicUniquePtr<T>& /*one*/, nx::utils::AtomicUniquePtr<T>& /*two*/)
{
    // DependentFalse is needed for assert to work at template instantiation time, not at definition time.
    static_assert(
        DependentFalse<T>::value,
        "There is no swap implementation for nx::utils::AtomicUniquePtr yet. Use std::move");
}

} // namespace std
