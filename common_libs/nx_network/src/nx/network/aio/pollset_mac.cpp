/**********************************************************
* 31 oct 2012
* a.kolesnikov
* PollSet class implementation for macosx/freebsd
***********************************************************/

#if defined(__APPLE__) || defined(__FreeBSD__)

#include "pollset.h"

#include <limits>
#include <set>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include "../system_socket.h"


//TODO #ak get rid of PollSetImpl::monitoredEvents due to performance cosiderations

using namespace std;

namespace nx {
namespace network {
namespace aio {
    static const int MAX_EVENTS_TO_RECEIVE = 32;

    class PollSetImpl
    {
    public:
        //!map<pair<socket, event type> >
        typedef std::set<std::pair<Pollable*, aio::EventType> > MonitoredEventSet;

        int kqueueFD;
        MonitoredEventSet monitoredEvents;
        int receivedEventCount;
        struct kevent receivedEventlist[MAX_EVENTS_TO_RECEIVE];
        //!linux event, used to interrupt epoll_wait
        int eventFD;

        PollSetImpl()
        :
            kqueueFD( -1 ),
            receivedEventCount( 0 ),
            eventFD( -1 )
        {
            memset( &receivedEventlist, 0, sizeof(receivedEventlist) );
        }

        ~PollSetImpl()
        {
        }
    };


    //////////////////////////////////////////////////////////
    // ConstIteratorImplOld
    //////////////////////////////////////////////////////////
    class ConstIteratorImplOld
    {
    public:
        PollSetImpl* pollSetImpl;
        int currentIndex;

        ConstIteratorImplOld()
        :
            pollSetImpl( NULL ),
            currentIndex( 0 )
        {
        }

        ConstIteratorImplOld( const ConstIteratorImplOld& right )
        :
            pollSetImpl( right.pollSetImpl ),
            currentIndex( right.currentIndex )
        {
        }

        const ConstIteratorImplOld& operator=( const ConstIteratorImplOld& right )
        {
            pollSetImpl = right.pollSetImpl;
            currentIndex = right.currentIndex;
            return *this;
        }
    };


    //////////////////////////////////////////////////////////
    // PollSet::const_iterator
    //////////////////////////////////////////////////////////
    PollSet::const_iterator::const_iterator()
    :
        m_impl( new ConstIteratorImplOld() )
    {
    }

    PollSet::const_iterator::const_iterator( const const_iterator& right )
    :
        m_impl( new ConstIteratorImplOld( *right.m_impl ) )
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
        m_impl->currentIndex++;
        while( m_impl->currentIndex < m_impl->pollSetImpl->receivedEventCount && 
               (m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].filter == EVFILT_USER ||
                m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].ident == (uintptr_t)-1) )  //socket has been removed
        {
            m_impl->currentIndex++;
        }
        return *this;
    }

    Pollable* PollSet::const_iterator::socket()
    {
        return static_cast<Pollable*>(m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].udata);
    }

    const Pollable* PollSet::const_iterator::socket() const
    {
        return static_cast<Pollable*>(m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].udata);
    }

    /*!
        \return bit mask of \a EventType
    */
    aio::EventType PollSet::const_iterator::eventType() const
    {
        const int filterType = m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].filter;
        const bool failure = (m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].flags & EV_ERROR) > 0;
        const bool eof = (m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].flags & EV_EOF) > 0;

        aio::EventType revents = aio::etNone;
        if( filterType == EVFILT_READ )
            revents = failure ? aio::etError : aio::etRead;
        else if( filterType == EVFILT_WRITE )
            revents = (failure || eof) ? aio::etError : aio::etWrite;
        return revents;
    }

    void* PollSet::const_iterator::userData()
    {
        return static_cast<Pollable*>(m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].udata)->impl()->eventTypeToUserData[eventType()];
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
    static const int USER_EVENT_IDENT = 11;

    PollSet::PollSet()
    :
        m_impl( new PollSetImpl() )
    {
        m_impl->kqueueFD = kqueue();
        if( m_impl->kqueueFD < 0 )
            return;

        //registering filter for interrupting poll
        struct kevent _newEvent;
        EV_SET( &_newEvent, USER_EVENT_IDENT, EVFILT_USER, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, NULL );
        kevent( m_impl->kqueueFD, &_newEvent, 1, NULL, 0, NULL );
    }

    PollSet::~PollSet()
    {
        if( m_impl->kqueueFD > 0 )
        {
            close( m_impl->kqueueFD );
            m_impl->kqueueFD = -1;
        }
        delete m_impl;
        m_impl = NULL;
    }

    bool PollSet::isValid() const
    {
        return m_impl->kqueueFD > 0;
    }

    //!Interrupts \a poll method, blocked in other thread
    /*!
        This is the only method which is allowed to be called from different thread
    */
    void PollSet::interrupt()
    {
        struct kevent _newEvent;
        EV_SET( &_newEvent, USER_EVENT_IDENT, EVFILT_USER, EV_CLEAR, NOTE_TRIGGER, 0, NULL );
        kevent( m_impl->kqueueFD, &_newEvent, 1, NULL, 0, NULL );
    }

    //!Add socket to set. Does not take socket ownership
    bool PollSet::add( Pollable* const sock, EventType eventType, void* userData )
    {
        pair<PollSetImpl::MonitoredEventSet::iterator, bool> p = m_impl->monitoredEvents.insert( make_pair( sock, eventType ) );
        if( !p.second )
            return true;    //event evenType is already listened on socket

        const short kfilterType = eventType == etRead ? EVFILT_READ : EVFILT_WRITE;

        //adding new fd to set
        struct kevent _newEvent;
        EV_SET( &_newEvent, sock->handle(), kfilterType, EV_ADD | EV_ENABLE, 0, 0, sock );
        if( kevent( m_impl->kqueueFD, &_newEvent, 1, NULL, 0, NULL ) == 0 )
        {
            sock->impl()->eventTypeToUserData[eventType] = userData;
            return true;
        }

        m_impl->monitoredEvents.erase( p.first );
        return false;
    }

    //!Remove socket from set
    void PollSet::remove( Pollable* const sock, EventType eventType )
    {
        PollSetImpl::MonitoredEventSet::iterator it = m_impl->monitoredEvents.find( make_pair( sock, eventType ) );
        if( it == m_impl->monitoredEvents.end() )
            return;

        const int kfilterType = eventType == etRead ? EVFILT_READ : EVFILT_WRITE;

        struct kevent _eventChange;
        EV_SET( &_eventChange, sock->handle(), kfilterType, EV_DELETE, 0, 0, NULL );
        kevent( m_impl->kqueueFD, &_eventChange, 1, NULL, 0, NULL );  //ignoring return code, since event is removed in any case
        m_impl->monitoredEvents.erase( it );

        //erasing socket from occured events array
        //TODO #ak better remove this linear run
        for( size_t i = 0; i < m_impl->receivedEventCount; ++i )
        {
            if( m_impl->receivedEventlist[i].ident == sock->handle() && m_impl->receivedEventlist[i].filter == kfilterType )
                m_impl->receivedEventlist[i].ident = (uintptr_t)-1;
        }

        //TODO #ak must not save user data in socket
        sock->impl()->eventTypeToUserData[eventType] = NULL;
    }

    size_t PollSet::size() const
    {
        return m_impl->monitoredEvents.size();
    }

    static const int INTERRUPT_CHECK_TIMEOUT_MS = 100;
    static const int MILLIS_IN_SEC = 1000;
    static const int NSEC_IN_MS = 1000000;

    /*!
        \param millisToWait if 0, method returns immediately. If > 0, returns on event or after \a millisToWait milliseconds.
            If < 0, returns after occuring of event
        \return -1 on error, 0 if \a millisToWait timeout has expired, > 0 - number of socket whose state has been changed
    */
    int PollSet::poll( int millisToWait )
    {
        memset( &m_impl->receivedEventlist, 0, sizeof(m_impl->receivedEventlist) );
        struct timespec timeout;
        if( millisToWait >= 0 )
        {
            memset( &timeout, 0, sizeof(timeout) );
            timeout.tv_sec = millisToWait / MILLIS_IN_SEC;
            timeout.tv_nsec = (millisToWait % MILLIS_IN_SEC) * NSEC_IN_MS;
        }
        m_impl->receivedEventCount = 0;
        int result = kevent(
            m_impl->kqueueFD,
            NULL,
            0,
            m_impl->receivedEventlist, 
            sizeof(m_impl->receivedEventlist)/sizeof(*m_impl->receivedEventlist),
            millisToWait >= 0 ? &timeout : NULL );
        if( result == EINTR )
            return 0;   //TODO #ak repeat wait (with timeout correction) in this case
        if( result <= 0 )
            return result;

        m_impl->receivedEventCount = result;
        //not reporting event used to interrupt blocking poll
        for( int i = 0; i < result; ++i )
        {
            if( m_impl->receivedEventlist[i].filter == EVFILT_USER )
                --result;
        }

        return result;
    }

    //!Returns iterator pointing to first socket, which state has been changed in previous \a poll call
    PollSet::const_iterator PollSet::begin() const
    {
        const_iterator beginIter;
        beginIter.m_impl->pollSetImpl = m_impl;
        beginIter.m_impl->currentIndex = -1;
        ++beginIter;  //moving to the first element (it may have non - 0 index in case of USER_EVENT)
        return beginIter;
    }

    //!Returns iterator pointing to next element after last socket, which state has been changed in previous \a poll call
    PollSet::const_iterator PollSet::end() const
    {
        const_iterator endIter;
        endIter.m_impl->pollSetImpl = m_impl;
        endIter.m_impl->currentIndex = m_impl->receivedEventCount;
        return endIter;
    }
}   // namespace aio
}   // namespace network
}   // namespace nx

#endif  //defined(__APPLE__) || defined(__FreeBSD__)
