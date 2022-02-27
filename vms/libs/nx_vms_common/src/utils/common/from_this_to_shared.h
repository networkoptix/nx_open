// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cassert>
#include <nx/utils/log/assert.h>

#include "core/resource/shared_resource_pointer.h"

/**
 * Enables conversion from \a this to shared pointer to class.
 */
template <class T>
class QnFromThisToShared
{
public:
    QnSharedResourcePointer<T> toSharedPointer() const
    {
        return m_weakPointer.toStrongRef();
    }

    QWeakPointer<T> weakPointer() const
    {
        return m_weakPointer;
    }

private:
    /** Weak reference to this, to make conversion to shared pointer possible. */
    QWeakPointer<T> m_weakPointer;

    /* Private API for QnSharedResourcePointer. */

    template<class T1>
    friend class QnSharedResourcePointer;

    template<class T2>
    void initWeakPointer(const QSharedPointer<T2>& pointer)
    {
        NX_ASSERT(!pointer.isNull());
        NX_ASSERT(m_weakPointer.toStrongRef().isNull(),
            "You have created two distinct shared pointers to a single object instance.");

        m_weakPointer = pointer;
    }

    void resetWeakPointer()
    {
        m_weakPointer.clear();
    }
};
