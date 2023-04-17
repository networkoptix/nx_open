// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

namespace nx {
namespace sdk {

/**
 * Smart pointer to objects that implement IRefCountable.
 *
 * Is assignment-compatible with smart pointers to derived classes.
 */
template<class RefCountable>
class Ptr final
{
public:
    /** Supports implicit conversion from nullptr. */
    Ptr(std::nullptr_t = nullptr)
    {
        // This assertion needs to be placed in any method because it uses sizeof().
        static_assert(sizeof(Ptr<RefCountable>) == sizeof(RefCountable*),
            "Ptr layout should be the same as of a raw pointer.");
    }

    explicit Ptr(RefCountable* ptr): m_ptr(ptr) {}

    template<class OtherRefCountable>
    explicit Ptr(OtherRefCountable* ptr): m_ptr(ptr) {}

    template<class OtherRefCountable>
    Ptr(const Ptr<OtherRefCountable>& other): m_ptr(other.get()) { addRef(); }

    /** Defined because the template above does not suppress generation of such member. */
    Ptr(const Ptr& other): m_ptr(other.get()) { addRef(); }

    template<class OtherRefCountable>
    Ptr(Ptr<OtherRefCountable>&& other): m_ptr(other.releasePtr()) {}

    /** Defined because the template above does not suppress generation of such member. */
    Ptr(Ptr&& other): m_ptr(other.releasePtr()) {}

    template<class OtherRefCountable>
    Ptr& operator=(const Ptr<OtherRefCountable>& other) { return assignConst(other); }

    /** Defined because the template above does not work for same-type assignment. */
    Ptr& operator=(const Ptr& other) { return assignConst(other); }

    template<class OtherRefCountable>
    Ptr& operator=(Ptr<OtherRefCountable>&& other) { return assignRvalue(std::move(other)); }

    /** Defined because the template above does not work for same-type assignment. */
    Ptr& operator=(Ptr&& other) { return assignRvalue(std::move(other)); }

    ~Ptr() { releaseRef(); }

    template<class OtherRefCountable>
    bool operator==(const Ptr<OtherRefCountable>& other) const { return m_ptr == other.get(); }

    template<class OtherRefCountable>
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
    template<class OtherRefCountable>
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
    RefCountable& operator*() const { return *m_ptr; }

    explicit operator bool() const { return m_ptr != nullptr; }

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

template<class RefCountable, typename Object>
bool operator==(const Ptr<RefCountable>& ptr, Object* p) { return ptr.get() == p; }

template<typename Object, class RefCountable>
bool operator==(Object* p, const Ptr<RefCountable>& ptr) { return p == ptr.get(); }

template<class RefCountable, typename Object>
bool operator!=(const Ptr<RefCountable>& ptr, Object* p) { return ptr.get() != p; }

template<typename Object, class RefCountable>
bool operator!=(Object* p, const Ptr<RefCountable>& ptr) { return p != ptr.get(); }

template<class RefCountable>
bool operator==(const Ptr<RefCountable>& ptr, std::nullptr_t) { return !ptr; }

template<class RefCountable>
bool operator==(std::nullptr_t, const Ptr<RefCountable>& ptr) { return !ptr; }

template<class RefCountable>
bool operator!=(const Ptr<RefCountable>& ptr, std::nullptr_t) { return (bool) ptr; }

template<class RefCountable>
bool operator!=(std::nullptr_t, const Ptr<RefCountable>& ptr) { return (bool) ptr; }

template<typename RefCountable>
bool operator<(const Ptr<const RefCountable>& first, const Ptr<const RefCountable>& second)
{
    return first.get() < second.get();
}

template<typename RefCountable>
bool operator<(const Ptr<const RefCountable>& refCountable, std::nullptr_t)
{
    return refCountable.get() < nullptr;
}

template<typename RefCountable>
bool operator<(std::nullptr_t, const Ptr<RefCountable>& refCountable)
{
    return nullptr < refCountable.get();
}

/**
 * Increments the reference counter and returns a new owning smart pointer. If refCountable is
 * null, just returns null.
 */
template<class RefCountable>
static Ptr<RefCountable> shareToPtr(RefCountable* refCountable)
{
    if (refCountable)
        refCountable->addRef();
    return Ptr(refCountable);
}

/**
 * Increments the reference counter and returns a new owning smart pointer. If ptr is null, just
 * returns null.
 */
template<class RefCountable>
static Ptr<RefCountable> shareToPtr(const Ptr<RefCountable>& ptr)
{
    return ptr; //< Invoke the copy constructor.
}

/**
 * Creates a new object via `new` (with reference count of 1) and returns a smart pointer to it.
 */
template<class RefCountable, typename... Args>
static Ptr<RefCountable> makePtr(Args&&... args)
{
    return Ptr(new RefCountable(std::forward<Args>(args)...));
}

/**
 * Calls queryInterface() from old SDK and returns a smart pointer to its result, possibly null.
 * @param refCountable Can be null, then null will be returned.
 */
template</*explicit*/ class Interface, /*deduced*/ class RefCountablePtr,
    /*deduced*/ typename OldInterfaceId>
static Ptr<Interface> queryInterfaceOfOldSdk(
    RefCountablePtr refCountable, const OldInterfaceId& interfaceId)
{
    return refCountable
        ? Ptr(static_cast<Interface*>(refCountable->queryInterface(interfaceId)))
        : nullptr;
}

/**
 * Intended for debug. Is not thread-safe.
 * @return Reference counter, or 0 if the pointer is null.
 */
template<class RefCountable>
int refCount(const Ptr<RefCountable>& ptr)
{
    return refCount(ptr.get());
}

} // namespace sdk
} // namespace nx
