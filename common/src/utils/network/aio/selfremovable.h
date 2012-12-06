/**********************************************************
* 28 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SELFREMOVABLE_H
#define SELFREMOVABLE_H

#include <QAtomicInt>


//!Inherit this class to enable object to be freed by callback (usefull for aio handler)
/*!
    Primary goal of this class is to allow inherited class object to be removed from its callback (e.g., from direct signal handler)
    \note Class methods are not thread-safe
    \note Descendant class should make its destructor private not to allow object to be created on stack and force clients to use \a scheduleForRemoval method
    \note This class is not thread-safe. That means it is not planned to use \a prohibitDestruction and \a allowDestruction in different threads
*/
class SelfRemovable
{
public:
    class ScopedDestructionProhibition
    {
    public:
        ScopedDestructionProhibition( SelfRemovable* const obj )
        :
            m_obj( obj )
        {
            m_obj->prohibitDestruction();
        }

        ~ScopedDestructionProhibition()
        {
            m_obj->allowDestruction();
        }

    private:
        SelfRemovable* const m_obj;
    };

    SelfRemovable();
    virtual ~SelfRemovable();

    //!Tells object to remove itself as soon as possible
    /*!
        If this method is call between \a prohibitDestruction() and \a allowDestruction() calls in current thread, then object destruction will be delayed
        until \a allowDestruction() call in current thread. 
        Otherwise, this method calls delete this
    */
    void scheduleForRemoval();

protected:
    //bool isScheduledForRemoval() const;

    //!Prohibites object destruction in current thread till \a allowDestruction call
    /*!
        \a prohibitDestruction and \a allowDestruction MUST be called from the same thread
    */
    void prohibitDestruction();
    //!Allow object destruction. If scheduleForRemoval() has been called after \a prohibitDestruction call, than this method will delete this
    /*!
        This call MUST follow \a prohibitDestruction call
    */
    void allowDestruction();

private:
    QAtomicInt m_counter;
};

#endif  //SELFREMOVABLE_H
