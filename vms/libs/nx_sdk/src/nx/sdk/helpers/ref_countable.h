// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <nx/sdk/helpers/lib_context.h>
#include <nx/sdk/i_ref_countable.h>

namespace nx::sdk {

/**
 * Not recommended to be used directly - use RefCountable, unless there is some special case.
 *
 * Implements IRefCountable reference counting. Can delegate reference counting to another
 * RefCountableHolder instance.
 *
 * Does not inherit IRefCountable because it would need to be a virtual base class.
 *
 * The instance is supposed to be nested into a ref-countable object, explicitly calling addRef()
 * and releaseRef() from the respective methods of that ref-countable class.
 */
class RefCountableHolder
{
public:
    RefCountableHolder(const RefCountableHolder&) = delete;
    RefCountableHolder& operator=(const RefCountableHolder&) = delete;
    RefCountableHolder(RefCountableHolder&&) = delete;
    RefCountableHolder& operator=(RefCountableHolder&&) = delete;
    ~RefCountableHolder() = default;

    /**
     * Takes ownership of the given object - it will be deleted when the reference counter
     * reaches zero.
     *
     * NOTE: After creation, the reference counter is 1.
     */
    RefCountableHolder(const IRefCountable* refCountable): m_refCountable(refCountable) {}

    /**
     * Delegates reference counting to another object. Does not change the reference counter.
     */
    RefCountableHolder(const RefCountableHolder* delegate): m_refCountHolderDelegate(delegate) {}

    int addRef() const
    {
        return m_refCountHolderDelegate ? m_refCountHolderDelegate->addRef() : ++m_refCount;
    }

    /**
     * NOTE: Deletes the owned object if the reference counter reaches zero.
     */
    int releaseRef() const
    {
        if (m_refCountHolderDelegate)
            return m_refCountHolderDelegate->releaseRef();

        const int newRefCounter = --m_refCount;
        if (newRefCounter == 0)
            delete m_refCountable;
        return newRefCounter;
    }

    int refCount() const
    {
        if (m_refCountHolderDelegate)
            return m_refCountHolderDelegate->refCount();
        return m_refCount;
    }

private:
    mutable std::atomic<int> m_refCount{1};
    const IRefCountable* const m_refCountable = nullptr;
    const RefCountableHolder* const m_refCountHolderDelegate = nullptr;
};

/**
 * Recommended base class for objects implementing an interface.
 *
 * Supports tracking the ref-countable objects via RefCountableRegistry.
 */
template<class RefCountableInterface>
class RefCountable: public RefCountableInterface
{
public:
    RefCountable(const RefCountable&) = delete;
    RefCountable& operator=(const RefCountable&) = delete;
    RefCountable(RefCountable&&) = delete;
    RefCountable& operator=(RefCountable&&) = delete;

    virtual ~RefCountable()
    {
        if (const auto refCountableRegistry = libContext().refCountableRegistry())
            refCountableRegistry->notifyDestroyed(this, refCount());
    }

    virtual int addRef() const override { return m_refCountableHolder.addRef(); }
    virtual int releaseRef() const override { return m_refCountableHolder.releaseRef(); }

    int refCount() const { return m_refCountableHolder.refCount(); }

protected:
    RefCountable(): m_refCountableHolder(static_cast<const IRefCountable*>(this))
    {
        if (const auto refCountableRegistry = libContext().refCountableRegistry())
            refCountableRegistry->notifyCreated(this, refCount());
    }

private:
    const RefCountableHolder m_refCountableHolder;
};

} // namespace nx::sdk
