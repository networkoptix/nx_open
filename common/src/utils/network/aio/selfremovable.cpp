/**********************************************************
* 28 nov 2012
* a.kolesnikov
***********************************************************/

#include "selfremovable.h"


SelfRemovable::SelfRemovable()
:
    m_counter( 1 )
{
}

SelfRemovable::~SelfRemovable()
{
}

void SelfRemovable::scheduleForRemoval()
{
    if( m_counter.fetchAndAddOrdered( -1 ) <= 1 )   //originally 1, current value - zero
        delete this;
}

//bool SelfRemovable::isScheduledForRemoval() const
//{
//    //TODO/IMPL
//    return false;
//}

//!Prohobites object destruction in current thread till \a allowDestruction call
/*!
    \a prohibitDestruction and \a allowDestruction MUST be called from the same thread
*/
void SelfRemovable::prohibitDestruction()
{
    m_counter.fetchAndAddOrdered( 1 );
}

//!Allow object destruction. If scheduleForRemoval() has been called after \a prohibitDestruction call, than this method will delete this
void SelfRemovable::allowDestruction()
{
    if( m_counter.fetchAndAddOrdered( -1 ) <= 1 )   //originally 1, current value - zero
        delete this;
}
