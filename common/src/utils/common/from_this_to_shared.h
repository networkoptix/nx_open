#ifndef FROM_THIS_TO_SHARED_H
#define FROM_THIS_TO_SHARED_H

#include "core/resource/shared_resource_pointer.h"


//!Enables conversion from \a this to shared pointer to class
/*!
    Implementation moved from QnResource class to enable it for non-QnResource classes
*/
template <class T>
class QnFromThisToShared
{
public:
    QnSharedResourcePointer<T> toSharedPointer() const
    {
        return m_weakPointer.toStrongRef();
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
        assert(!pointer.isNull());
        assert(m_weakPointer.toStrongRef().isNull()); /* Error in this line means that you have created two distinct shared pointers to a single resource instance. */

        m_weakPointer = pointer;
    }
};

#endif  //FROM_THIS_TO_SHARED_H
