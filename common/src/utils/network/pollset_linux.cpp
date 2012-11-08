/**********************************************************
* 31 oct 2012
* a.kolesnikov
* PollSet class implementation for linux
***********************************************************/

#ifdef Q_OS_LINUX

#include "pollset.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <map>

#include "socket.h"


using namespace std;

//////////////////////////////////////////////////////////
// PollSetImpl
//////////////////////////////////////////////////////////
class PollSetImpl
{
public:
    int epollSetFD;
    //!map<fd, events mask>
    std::map<Socket*, int> monitoredEvents;
    int signalledSockCount;
    size_t epollEventsArrayCapacity;
    epoll_event* epollEventsArray;
    //!linux event, used to interrupt epoll_wait
    int eventFD;

    PollSetImpl()
    :
        epollSetFD( -1 ),
        signalledSockCount( 0 ),
        epollEventsArrayCapacity( 32 ),
        epollEventsArray( new epoll_event[epollEventsArrayCapacity] ),
        eventFD( -1 )
    {
    }

    ~PollSetImpl()
    {
        delete[] epollEventsArray;
        epollEventsArray = NULL;
        epollEventsArrayCapacity = 0;
    }
};


//////////////////////////////////////////////////////////
// ConstIteratorImpl
//////////////////////////////////////////////////////////
class ConstIteratorImpl
{
public:
    int currentIndex;
    PollSetImpl* pollSetImpl;

    ConstIteratorImpl()
    :
        currentIndex( 0 ),
        pollSetImpl( NULL )
    {
    }

    ConstIteratorImpl( const ConstIteratorImpl& right )
    :
        currentIndex( right.currentIndex ),
        pollSetImpl( right.pollSetImpl )
    {
    }

    const ConstIteratorImpl& operator=( const ConstIteratorImpl& right )
    {
        currentIndex = right.currentIndex;
        pollSetImpl = right.pollSetImpl;
		return *this;
    }

    void selectNextSocket()
    {
        ++currentIndex;
        if( (currentIndex < pollSetImpl->signalledSockCount) && (pollSetImpl->epollEventsArray[currentIndex].data.ptr == NULL) )
        {
            //reading eventfd and selecting next socket
            uint64_t val = 0;
            if( read( pollSetImpl->eventFD, &val, sizeof(val) ) == -1 )
            {
                //TODO ???
            }
            ++currentIndex;
        }
    }
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
    delete[] m_impl;
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
    m_impl->selectNextSocket();
    return *this;
}

Socket* PollSet::const_iterator::socket() const
{
    return static_cast<Socket*>(m_impl->pollSetImpl->epollEventsArray[m_impl->currentIndex].data.ptr);
}

/*!
    \return bit mask of \a EventType
*/
unsigned int PollSet::const_iterator::revents() const
{
    const uint32_t epollREvents = m_impl->pollSetImpl->epollEventsArray[m_impl->currentIndex].events;
    unsigned int revents = 0;
    if( epollREvents & EPOLLIN )
        revents |= etRead;
    if( epollREvents & EPOLLOUT )
        revents |= etWrite;
    return revents;
}

bool PollSet::const_iterator::operator==( const const_iterator& right ) const
{
    return (m_impl->pollSetImpl == right.m_impl->pollSetImpl) && (m_impl->currentIndex == right.m_impl->currentIndex);
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
    m_impl->epollSetFD = epoll_create( 256 );
    m_impl->eventFD = eventfd( 0, EFD_NONBLOCK );

    if( m_impl->epollSetFD > 0 && m_impl->eventFD > 0 )
    {
        epoll_event _event;
        memset( &_event, 0, sizeof(_event) );
        _event.data.ptr = NULL; //m_impl->eventFD;
        _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        if( epoll_ctl( m_impl->epollSetFD, EPOLL_CTL_ADD, m_impl->eventFD, &_event ) )
        {
            close( m_impl->eventFD );
            m_impl->eventFD = -1;
        }
    }
}

PollSet::~PollSet()
{
    if( m_impl->epollSetFD > 0 )
    {
        close( m_impl->epollSetFD );
        m_impl->epollSetFD = -1;
    }
    if( m_impl->eventFD > 0 )
    {
        close( m_impl->eventFD );
        m_impl->eventFD = -1;
    }

    delete m_impl;
    m_impl = NULL;
}

bool PollSet::isValid() const
{
    return (m_impl->epollSetFD > 0) && (m_impl->eventFD > 0);
}

//!Interrupts \a poll method, blocked in other thread
/*!
    This is the only method which is allowed to be called from different thread
*/
void PollSet::interrupt()
{
    uint64_t val = 1;
    if( write( m_impl->eventFD, &val, sizeof(val) ) != sizeof(val) )
    {
        //TODO ???
    }
}

//!Add socket to set. Does not take socket ownership
bool PollSet::add( Socket* const sock, EventType eventType )
{
    const int epollEventType = eventType == etRead ? EPOLLIN : EPOLLOUT;

    pair<map<Socket*, int>::iterator, bool> p = m_impl->monitoredEvents.insert( make_pair( sock, 0 ) );
    if( p.second )
    {
        //adding new fd to set
        epoll_event _event;
        memset( &_event, 0, sizeof(_event) );
        _event.data.ptr = sock;
        _event.events = epollEventType | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        if( epoll_ctl( m_impl->epollSetFD, EPOLL_CTL_ADD, sock->handle(), &_event ) == 0 )
        {
            p.first->second = epollEventType;
            return true;
        }
        else
        {
            m_impl->monitoredEvents.erase( p.first );
            return false;
        }
    }
    else
    {
        if( p.first->second & epollEventType )
            return true;    //event evenType is already listened on socket

        epoll_event _event;
        memset( &_event, 0, sizeof(_event) );
        _event.data.ptr = sock;
        _event.events = epollEventType | p.first->second | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        if( epoll_ctl( m_impl->epollSetFD, EPOLL_CTL_MOD, sock->handle(), &_event ) == 0 )
        {
            p.first->second |= epollEventType;
            return true;
        }
        else
        {
            return false;
        }
    }
}

//!Remove socket from set
void PollSet::remove( Socket* const sock, EventType eventType )
{
    const int epollEventType = eventType == etRead ? EPOLLIN : EPOLLOUT;
    map<Socket*, int>::iterator it = m_impl->monitoredEvents.find( sock );
    if( (it == m_impl->monitoredEvents.end()) || !(it->second & epollEventType) )
        return;

    const int eventsBesidesRemovedOne = it->second & (~epollEventType);
    if( eventsBesidesRemovedOne )
    {
        //socket is being listened for multiple events, modifying event set
        epoll_event _event;
        memset( &_event, 0, sizeof(_event) );
        _event.data.ptr = sock;
        _event.events = eventsBesidesRemovedOne | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        if( epoll_ctl( m_impl->epollSetFD, EPOLL_CTL_MOD, sock->handle(), &_event ) == 0 )
            it->second = eventsBesidesRemovedOne;
    }
    else
    {
        //socket is being listened for epollEventType only, removing socket...
        epoll_ctl( m_impl->epollSetFD, EPOLL_CTL_DEL, sock->handle(), NULL );
        m_impl->monitoredEvents.erase( it );
    }
}

static const int INTERRUPT_CHECK_TIMEOUT_MS = 100;

/*!
    \param millisToWait if 0, method returns immediatly. If > 0, returns on event or after \a millisToWait milliseconds.
        If < 0, returns after occuring of event
    \return -1 on error, 0 if \a millisToWait timeout has expired, > 0 - number of socket whose state has been changed
*/
int PollSet::poll( int millisToWait )
{
    int result = epoll_wait(
        m_impl->epollSetFD,
        m_impl->epollEventsArray,
        m_impl->epollEventsArrayCapacity,
        millisToWait < 0 ? -1 : millisToWait );
    m_impl->signalledSockCount = result < 0 ? 0 : result;
    return result;
}

//!Returns iterator pointing to first socket, which state has been changed in previous \a poll call
PollSet::const_iterator PollSet::begin() const
{
    const_iterator _begin;
    _begin.m_impl->currentIndex = -1;
    _begin.m_impl->pollSetImpl = m_impl;
    _begin.m_impl->selectNextSocket();
    return _begin;
}

//!Returns iterator pointing to next element after last socket, which state has been changed in previous \a poll call
PollSet::const_iterator PollSet::end() const
{
    const_iterator _end;
    _end.m_impl->currentIndex = m_impl->signalledSockCount;
    _end.m_impl->pollSetImpl = m_impl;
    return _end;
}

unsigned int PollSet::maxPollSetSize()
{
    //TODO/IMPL
    return 128;
}

#endif
