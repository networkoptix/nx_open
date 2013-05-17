/**********************************************************
* 31 oct 2012
* a.kolesnikov
* PollSet class implementation for macosx
***********************************************************/

#ifdef __APPLE__

#include "pollset.h"

#include <map>

#include "../socket.h"


using namespace std;

//////////////////////////////////////////////////////////
// PollSetImpl
//////////////////////////////////////////////////////////
class PollSetImpl
{
public:
};


//////////////////////////////////////////////////////////
// ConstIteratorImpl
//////////////////////////////////////////////////////////
class ConstIteratorImpl
{
public:
};


//////////////////////////////////////////////////////////
// PollSet::const_iterator
//////////////////////////////////////////////////////////
PollSet::const_iterator::const_iterator()
:
    m_impl( new ConstIteratorImpl() )
{
}

PollSet::const_iterator::const_iterator( const const_iterator& right )
:
    m_impl( new ConstIteratorImpl( *right.m_impl ) )
{
}

PollSet::const_iterator::~const_iterator()
{
    delete m_impl;
    m_impl = NULL;
}

PollSet::const_iterator& PollSet::const_iterator::operator=( const const_iterator& right )
{
    *m_impl = *right.m_impl;
    return *this;
}

//!Selects next socket which state has been changed with previous \a poll call
PollSet::const_iterator PollSet::const_iterator::operator++(int)    //it++
{
    const_iterator bak( *this );
    ++(*this);
    return bak;
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
PollSet::EventType PollSet::const_iterator::eventType() const
{
    //TODO/IMPL
    return PollSet::etRead;
}

void* PollSet::const_iterator::userData()
{
    //TODO/IMPL
    return NULL;
}

bool PollSet::const_iterator::operator==( const const_iterator& right ) const
{
    //TODO/IMPL
    return true;
}

bool PollSet::const_iterator::operator!=( const const_iterator& right ) const
{
    return !(*this == right);
}


//////////////////////////////////////////////////////////
// PollSet
//////////////////////////////////////////////////////////
PollSet::PollSet()
:
    m_impl( new PollSetImpl() )
{
    //TODO/IMPL
}

PollSet::~PollSet()
{
    //TODO/IMPL
}

bool PollSet::isValid() const
{
    //TODO/IMPL
    return false;
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
bool PollSet::add( Socket* const sock, EventType eventType, void* userData )
{
    //TODO/IMPL
    return false;
}

//!Remove socket from set
void* PollSet::remove( Socket* const sock, EventType eventType )
{
    //TODO/IMPL
    return NULL;
}

size_t PollSet::size( EventType /*eventType*/ ) const
{
    //TODO/IMPL
    return 0;
}

void* PollSet::getUserData( Socket* const sock, EventType eventType ) const
{
    //TODO/IMPL
    return NULL;
}

static const int INTERRUPT_CHECK_TIMEOUT_MS = 100;

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

unsigned int PollSet::maxPollSetSize()
{
    //TODO/IMPL
    return 128;
}

#endif  //__APPLE__
