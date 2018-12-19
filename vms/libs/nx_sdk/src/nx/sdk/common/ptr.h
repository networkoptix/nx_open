#pragma once

#include <cstddef>
#include <type_traits>

namespace nx {
namespace sdk {
namespace common {

/**
 * Smart pointer to objects that implement IRefCountable.
 *
 * Supports dynamic-cast, and is assignment-compatible with smart pointers to derived classes.
 */
template<typename RefCountable>
class Ptr final
{
public:
    template<typename... Args>
    static auto make(Args&&... args)
    {
        return Ptr<RefCountable>{new RefCountable(std::forward<Args>(args)...)};
    }

    /** Supports implicit conversion from nullptr. */
    Ptr(std::nullptr_t = nullptr) {}

    /** Intended to be applied to queryInterface(). */
    explicit Ptr(void* ptr): Ptr(static_cast<RefCountable*>(ptr)) {}

    /** Intended to be applied to queryInterface(). */
    explicit Ptr(const void* ptr): Ptr(static_cast<RefCountable*>(ptr)) {}

    /** Sfinae: Compiles if OtherRefCountable* is convertible to RefCountable*. */
    template<class OtherRefCountable>
    using IsConvertibleFrom =
        std::enable_if_t<std::is_base_of<RefCountable, OtherRefCountable>::value, int>;

    template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    explicit Ptr(OtherRefCountable* ptr): m_ptr(ptr)
    {
    }

    /** Constructor template is never a copy constructor, thus, need to define it explicitly. */
    Ptr(const Ptr<RefCountable>& other): m_ptr(other.get())
    {
        if (m_ptr)
            m_ptr->addRef();
    }

	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    Ptr(const Ptr<OtherRefCountable>& other): m_ptr(other.get())
    {
        if (m_ptr)
            m_ptr->addRef();
    }

	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    Ptr(Ptr<OtherRefCountable>&& other): m_ptr(other.get())
    {
        other.m_ptr = nullptr;
    }

	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    Ptr& operator=(const Ptr<OtherRefCountable>& other)
    {
        if (this == &other)
            return *this;

        m_ptr = other.get();
        if (m_ptr)
            m_ptr->addRef();
        return *this;
    }

	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    Ptr& operator=(Ptr<OtherRefCountable>&& other)
    {
        if (this == &other)
            return *this;

        if (m_ptr)
            m_ptr->releaseRef();
        m_ptr = other.releasePtr();
        return *this;
    }

    ~Ptr()
    {
        if (m_ptr)
            m_ptr->releaseRef();
    }

	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    bool operator==(const Ptr<OtherRefCountable>& other) const { return m_ptr == other.get(); }

	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    bool operator!=(const Ptr<OtherRefCountable>& other) const { return m_ptr != other.get(); }

    /**
     * Decrements the reference counter of the owned object (will be deleted if reaches 0), and
     * replaces the stored pointer with null.
     */
    void reset()
    {
        if (m_ptr)
            m_ptr->releaseRef();
        m_ptr = nullptr;
    }

    /**
     * Decrements the reference counter of the owned object (will be deleted if reaches 0), and
     * starts owning the specified object, leaving its reference counter intact.
     */
	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    void reset(OtherRefCountable* ptr)
    {
        if (m_ptr)
            m_ptr->releaseRef();
        m_ptr = ptr;
    }

    /**
     * Cancels ownership of the object: replaces the stored pointer with null. The reference
     * counter of the object remains intact.
     * @return Object pointer.
     */
    RefCountable* releasePtr()
    {
        RefCountable* result = m_ptr;
        m_ptr = nullptr;
        return result;
    }

    RefCountable* get() const { return m_ptr;  }
    RefCountable* operator->() const { return m_ptr; }

    operator bool() const { return m_ptr != nullptr; }

    template<class OtherRefCountable>
    Ptr<OtherRefCountable> dynamicCast()
    {
        auto ptr = dynamic_cast<OtherRefCountable*>(m_ptr);
        if (ptr)
            m_ptr->addRef();
        return Ptr<OtherRefCountable>(ptr);
    }

private:
    RefCountable* m_ptr = nullptr;
};

} // namespace common
} // namespace sdk
} // namespace nx
