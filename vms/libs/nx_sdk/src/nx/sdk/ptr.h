// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utility>
#include <cstddef>
#include <type_traits>

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

    operator bool() const { return m_ptr != nullptr; }

private:
    template<class OtherRefCountable>
    friend Ptr<OtherRefCountable> toPtr(OtherRefCountable* refCountable);

    explicit Ptr(RefCountable* ptr): m_ptr(ptr) {}

    template<class OtherRefCountable>
    explicit Ptr(OtherRefCountable* ptr): m_ptr(ptr) {}

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

template<class RefCountable, typename Pointer>
bool operator==(const Ptr<RefCountable>& ptr, Pointer* p) { return ptr.get() == p; }

template<typename Pointer, class RefCountable>
bool operator==(Pointer* p, const Ptr<RefCountable>& ptr) { return p == ptr.get(); }

template<class RefCountable, typename Pointer>
bool operator!=(const Ptr<RefCountable>& ptr, Pointer* p) { return ptr.get() != p; }

template<typename Pointer, class RefCountable>
bool operator!=(Pointer* p, const Ptr<RefCountable>& ptr) { return p != ptr.get(); }

template<class RefCountable>
bool operator==(const Ptr<RefCountable>& ptr, std::nullptr_t) { return !ptr; }

template<class RefCountable>
bool operator==(std::nullptr_t, const Ptr<RefCountable>& ptr) { return !ptr; }

template<class RefCountable>
bool operator!=(const Ptr<RefCountable>& ptr, std::nullptr_t) { return (bool) ptr; }

template<class RefCountable>
bool operator!=(std::nullptr_t, const Ptr<RefCountable>& ptr) { return (bool) ptr; }

/**
 * Wrapper for Ptr constructor - needed to use the template argument deduction.
 *
 * NOTE: Will not be needed after migrating to C++17 because of the support for constructor
 * template argument deduction.
 */
template<class RefCountable>
static Ptr<RefCountable> toPtr(RefCountable* refCountable)
{
    return Ptr<RefCountable>(refCountable);
}

/**
 * Creates a new object via `new` (with reference count of 1) and returns a smart pointer to it.
 */
template<class RefCountable, typename... Args>
static Ptr<RefCountable> makePtr(Args&&... args)
{
    return toPtr(new RefCountable(std::forward<Args>(args)...));
}

/**
 * Calls queryInterface() from old SDK and returns a smart pointer to its result, possibly null.
 * @param refCountable Can be null, then null will be returned.
 */
template</*explicit*/ class Interface, /*deduced*/ class RefCountablePtr,
    /*deduced*/ typename InterfaceId>
static Ptr<Interface> queryInterfaceOfOldSdk(
    RefCountablePtr refCountable, const InterfaceId& interfaceId)
{
    return refCountable
        ? toPtr(static_cast<Interface*>(refCountable->queryInterface(interfaceId)))
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

//-------------------------------------------------------------------------------------------------

/**
 * Non-owning pointer to objects that implement IRefCountable: it neither calls addRef() in the
 * constructor, nor calls releaseRef() in the destructor. It can be created from both raw and smart
 * pointers (without altering the reference counter), and can be implicitly converted to a smart
 * pointer Ptr<>(), which is the only case when it calls addRef().
 *
 * The above features allow to use RawPtr for function arguments, because by the SDK guidelines,
 * functions are supplied objects with their original reference counter values, and should
 * increment them only when the object is intended to survive the function call, e.g. being
 * assigned to a smart pointer.
 */
template<class RefCountable>
class RawPtr
{
public:
    RawPtr(std::nullptr_t = nullptr): m_ptr(nullptr) {}

    RawPtr(RefCountable* refCountable): m_ptr(refCountable) {}

    template<class OtherRefCountable>
    RawPtr(const Ptr<OtherRefCountable>& ptr): m_ptr(ptr.get()) {}

    template<class OtherRefCountable>
    RawPtr(const RawPtr<OtherRefCountable>& other): m_ptr(other.get()) {}

    /** Defined because the template above does not suppress generation of such member. */
    RawPtr(const RawPtr& other): m_ptr(other.get()) {}

    template<class OtherRefCountable>
    RawPtr(RawPtr<OtherRefCountable>&& other): m_ptr(other.get()) {}

    /** Defined because the template above does not suppress generation of such member. */
    RawPtr(RawPtr&& other): m_ptr(other.get()) {}

    template<class OtherRefCountable>
    RawPtr& operator=(const RawPtr<OtherRefCountable>& other) { m_ptr = other.get(); return *this; }

    /** Defined because the template above does not work for same-type assignment. */
    RawPtr& operator=(const RawPtr& other) { m_ptr = other.get(); return *this; }

    template<class OtherRefCountable>
    RawPtr& operator=(RawPtr<OtherRefCountable>&& other) { m_ptr = other.get(); return *this; }

    /** Defined because the template above does not work for same-type assignment. */
    RawPtr& operator=(RawPtr&& other) { m_ptr = other.get(); return *this; }

    template<class OtherRefCountable>
    bool operator==(const RawPtr<OtherRefCountable>& other) const { return m_ptr == other.get(); }

    template<class OtherRefCountable>
    bool operator!=(const RawPtr<OtherRefCountable>& other) const { return !operator==(other); }

    RefCountable* operator->() const { return m_ptr; }
    RefCountable& operator*() const { return *m_ptr; }
    RefCountable* get() const { return m_ptr; }

    operator bool() const { return m_ptr != nullptr; }

    operator Ptr<RefCountable>() const
    {
        m_ptr->addRef();
        return toPtr(m_ptr);
    }

private:
    RefCountable* const m_ptr;
};

template<class RefCountable, typename Pointer>
bool operator==(const RawPtr<RefCountable>& ptr, Pointer* p) { return ptr.get() == p; }

template<typename Pointer, class RefCountable>
bool operator==(Pointer* p, const RawPtr<RefCountable>& ptr) { return p == ptr.get(); }

template<class RefCountable, typename Pointer>
bool operator!=(const RawPtr<RefCountable>& ptr, Pointer* p) { return ptr.get() != p; }

template<typename Pointer, class RefCountable>
bool operator!=(Pointer* p, const RawPtr<RefCountable>& ptr) { return p != ptr.get(); }

template<class RefCountable>
bool operator==(const RawPtr<RefCountable>& ptr, std::nullptr_t) { return !ptr; }

template<class RefCountable>
bool operator==(std::nullptr_t, const RawPtr<RefCountable>& ptr) { return !ptr; }

template<class RefCountable>
bool operator!=(const RawPtr<RefCountable>& ptr, std::nullptr_t) { return (bool) ptr; }

template<class RefCountable>
bool operator!=(std::nullptr_t, const RawPtr<RefCountable>& ptr) { return (bool) ptr; }

} // namespace sdk
} // namespace nx
