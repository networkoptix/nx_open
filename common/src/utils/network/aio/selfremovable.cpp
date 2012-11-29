/**********************************************************
* 28 nov 2012
* a.kolesnikov
***********************************************************/

#include "selfremovable.h"


SelfRemovable::SelfRemovable()
:
    m_scheduledForRemoval( false )
{
}

void SelfRemovable::scheduleForRemoval()
{
    m_scheduledForRemoval = true;
}

bool SelfRemovable::isScheduledForRemoval() const
{
    return m_scheduledForRemoval;
}
