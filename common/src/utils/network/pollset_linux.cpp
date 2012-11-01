/**********************************************************
* 31 oct 2012
* a.kolesnikov
* PollSet class implementation for linux
***********************************************************/

#ifdef Q_OS_LINUX

#include "pollset.h"

#include "socket.h"


//!Selects next socket which state has been changed with previous \a poll call
PollSet::const_iterator& PollSet::const_iterator::operator++(int)    //it++
{
    //TODO/IMPL
    return *this;
}

//!Selects next socket which state has been changed with previous \a poll call
PollSet::const_iterator& PollSet::const_iterator::operator++()       //++it
{
    //TODO/IMPL
    return *this;
}

Socket* PollSet::const_iterator::socket() const
{
    //TODO/IMPL
    return NULL;
}

/*!
    \return bit mask of \a EventType
*/
unsigned int PollSet::const_iterator::revents() const
{
    //TODO/IMPL
    return 0;
}

bool PollSet::const_iterator::operator==( const const_iterator& right ) const
{
    //TODO/IMPL
    return false;
}

bool PollSet::const_iterator::operator!=( const const_iterator& right ) const
{
    return !(*this == right);
}


PollSet::PollSet()
{
}

PollSet::~PollSet()
{
}

//!Interrupts \a poll method, blocked in other thread
/*!
    This is the only method which is allowed to be called from different thread
*/
void PollSet::interrupt()
{
    //TODO/IMPL
}

//!Add socket to set. Does not take socket ownership
bool PollSet::add( Socket* const sock )
{
    //TODO/IMPL
    return false;
}

//!Remove socket from set
void PollSet::remove( Socket* const sock )
{
    //TODO/IMPL
}

/*!
    \param millisToWait if 0, method returns immediatly. If > 0, returns on event or after \a millisToWait milliseconds.
        If < 0, returns after occuring of event
    \return -1 on error, 0 if \a millisToWait timeout has expired, > 0 - number of socket whose state has been changed
*/
int PollSet::poll( int millisToWait )
{
    //TODO/IMPL
    return 0;
}

//!Returns iterator pointing to first socket, which state has been changed in previous \a poll call
PollSet::const_iterator PollSet::begin() const
{
    //TODO/IMPL
    return const_iterator();
}

//!Returns iterator pointing to next element after last socket, which state has been changed in previous \a poll call
PollSet::const_iterator PollSet::end() const
{
    //TODO/IMPL
    return const_iterator();
}

QString PollSet::lastErrorText() const
{
    //TODO/IMPL
    return QString();
}

unsigned int PollSet::maxPollSetSize()
{
    //TODO/IMPL
    return 0;
}

#endif
