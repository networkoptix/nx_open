#pragma once

#include <utility>
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
    /** Supports implicit conversion from nullptr. */
    Ptr(std::nullptr_t = nullptr) {}

    /** Intended to be applied to queryInterface(). */
    explicit Ptr(void* ptr): Ptr(static_cast<RefCountable*>(ptr)) {}

    /** Intended to be applied to queryInterface(). */
    explicit Ptr(const void* ptr): Ptr(static_cast<RefCountable*>(ptr)) {}

    /** Sfinae: Compiles if OtherRefCountable* is convertible to RefCountable*. */
    template<class OtherRefCountable>
    using IsConvertibleFrom =
        std::enable_if_t<std::is_base_of<RefCountable, OtherRefCountable>::value, int /*dummy*/>;

    template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    explicit Ptr(OtherRefCountable* ptr): m_ptr(ptr) {}

	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    Ptr(const Ptr<OtherRefCountable>& other): m_ptr(other.get()) { addRef(); }

    /** Defined because the template above does not suppress generation of such member. */
    Ptr(const Ptr& other): m_ptr(other.get()) { addRef(); }

    template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    Ptr(Ptr<OtherRefCountable>&& other): m_ptr(other.releasePtr()) {}

    /** Defined because the template above does not suppress generation of such member. */
    Ptr(Ptr&& other): m_ptr(other.releasePtr()) {}

    template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    Ptr& operator=(const Ptr<OtherRefCountable>& other) { return assignConst(other); }

    /** Defined because the template above does not work for same-type assignment. */
    Ptr& operator=(const Ptr& other) { return assignConst(other); }

	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    Ptr& operator=(Ptr<OtherRefCountable>&& other) { return assignRvalue(std::move(other)); }

    /** Defined because the template above does not work for same-type assignment. */
    Ptr& operator=(Ptr&& other) { return assignRvalue(std::move(other)); }

    ~Ptr() { releaseRef(); }

	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    bool operator==(const Ptr<OtherRefCountable>& other) const { return m_ptr == other.get(); }

	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    bool operator!=(const Ptr<OtherRefCountable>& other) const { return !operator==(other); }

    /**
     * Decrements the reference counter of the owned object (will be deleted if reaches 0), and
     * replaces the stored pointer with null.
     */
    void reset()
    {
        releaseRef();
        m_ptr = nullptr;
    }

    /**
     * Decrements the reference counter of the owned object (will be deleted if reaches 0), and
     * starts owning the specified object, leaving its reference counter intact.
     */
	template<class OtherRefCountable, IsConvertibleFrom<OtherRefCountable> = 0>
    void reset(OtherRefCountable* ptr)
    {
        releaseRef();
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
    Ptr<OtherRefCountable> dynamicCast() const
    {
        auto ptr = dynamic_cast<OtherRefCountable*>(m_ptr);
        if (ptr)
            m_ptr->addRef();
        return Ptr<OtherRefCountable>(ptr);
    }

private:
    void addRef()
    {
        if (m_ptr)
            m_ptr->addRef();
    }

    void releaseRef()
    {
        if (m_ptr)
            m_ptr->releaseRef();
    }

    Ptr& assignConst(const Ptr& other)
    {
        if (this != &other && m_ptr != other.get())
        {
            releaseRef();
            m_ptr = other.get();
            addRef();
        }
        return *this;
    }

    Ptr& assignRvalue(Ptr&& other)
    {
        if (this != &other && m_ptr != other.get())
        {
            releaseRef();
            m_ptr = other.releasePtr();
        }
        return *this;
    }

private:
    RefCountable* m_ptr = nullptr;
};

template<typename RefCountable, typename... Args>
static Ptr<RefCountable> makePtr(Args&&... args)
{
    return Ptr<RefCountable>{new RefCountable(std::forward<Args>(args)...)};
}

} // namespace common
} // namespace sdk
} // namespace nx
