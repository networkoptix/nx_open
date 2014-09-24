/**********************************************************
* 31 oct 2012
* a.kolesnikov
* PollSet class implementation for linux
***********************************************************/

#ifdef __linux__

#include <qglobal.h>

#include "pollset.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <map>

#include "../system_socket.h"


using namespace std;

namespace aio
{

    //////////////////////////////////////////////////////////
    // PollSetImpl
    //////////////////////////////////////////////////////////
    class PollSetImpl
    {
    public:
        struct SockData
        {
            //!bit 'or' of listened events (EPOLLIN, EPOLLOUT, etc..)
            int eventsMask;
            bool markedForRemoval;

            SockData()
            :
                eventsMask( 0 ),
                markedForRemoval( false )
            {
            }

            SockData(
                int _eventsMask,
                bool _markedForRemoval )
            :
                eventsMask( _eventsMask ),
                markedForRemoval( _markedForRemoval )
            {
            }

            const void* userData( aio::EventType eventType ) const
            {
                //TODO #ak: get rid of m_userData map, since there can be only two(!) elements in it
                std::map<aio::EventType, void*>::const_iterator it = m_userData.find(eventType);
                return it != m_userData.end() ? it->second : NULL;
            }

            void*& userData( aio::EventType eventType )
            {
                return m_userData[eventType];
            }

        private:
            std::map<aio::EventType, void*> m_userData;
        };

        //!map<fd, pair<events mask, user data> >
        typedef std::map<Socket*, SockData> MonitoredEventMap;

        int epollSetFD;
        MonitoredEventMap monitoredEvents;
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
        aio::EventType triggeredEvent;
        aio::EventType handlerToUse;

        ConstIteratorImpl()
        :
            currentIndex( 0 ),
            pollSetImpl( NULL ),
            triggeredEvent( aio::etNone ),
            handlerToUse( aio::etNone )
        {
        }

        void selectNextSocket()
        {
            for( ; currentIndex < pollSetImpl->signalledSockCount; )
            {
                PollSetImpl::MonitoredEventMap::const_pointer socketData = NULL;
                if( currentIndex >= 0 )
                {
                    socketData = static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(pollSetImpl->epollEventsArray[currentIndex].data.ptr);
                    //reporting EPOLLIN and EPOLLOUT as different events to allow them to have different handlers:
                    if( socketData != NULL &&
                        !socketData->second.markedForRemoval &&
                        handlerToUse == aio::etRead &&
                        (pollSetImpl->epollEventsArray[currentIndex].events & EPOLLOUT) )
                    {
                        //if both EPOLLIN and EPOLLOUT have occured and EPOLLIN has already been reported then reporting EPOLLOUT
                        handlerToUse = aio::etWrite;
                        triggeredEvent = (pollSetImpl->epollEventsArray[currentIndex].events & EPOLLERR)
                            ? aio::etError
                            : aio::etWrite;
                        return;
                    }
                }

                ++currentIndex;
                if( currentIndex >= pollSetImpl->signalledSockCount ) 
                    return; //reached end, iterator is invalid

                if( pollSetImpl->epollEventsArray[currentIndex].data.ptr == NULL )
                {
                    //reading eventfd and selecting next socket
                    uint64_t val = 0;
                    if( read( pollSetImpl->eventFD, &val, sizeof(val) ) == -1 )
                    {
                        //TODO #ak: can this really occur???
                    }
                    continue;
                }

                socketData = static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(pollSetImpl->epollEventsArray[currentIndex].data.ptr);
                if( socketData->second.markedForRemoval )
                    continue;   //skipping socket

                //processing all triggered events one by one
                if( pollSetImpl->epollEventsArray[currentIndex].events & EPOLLIN )
                {
                    handlerToUse = aio::etRead;
                    triggeredEvent = aio::etRead;
                }
                else if( pollSetImpl->epollEventsArray[currentIndex].events & EPOLLOUT )
                {
                    handlerToUse = aio::etWrite;
                    triggeredEvent = aio::etWrite;
                }
                else if( pollSetImpl->epollEventsArray[currentIndex].events & (EPOLLHUP | EPOLLRDHUP) )
                {
                    //reporting events to every one who listens
                    pollSetImpl->epollEventsArray[currentIndex].events |= socketData->second.eventsMask & (EPOLLIN | EPOLLOUT);
                    handlerToUse = (socketData->second.eventsMask & EPOLLIN) ? aio::etRead : aio::etWrite;
                    triggeredEvent = handlerToUse;
                }
                else if( pollSetImpl->epollEventsArray[currentIndex].events & EPOLLERR )
                {
                    //reporting aio::etError to every one who listens
                    pollSetImpl->epollEventsArray[currentIndex].events |= socketData->second.eventsMask & (EPOLLIN | EPOLLOUT);
                    handlerToUse = (socketData->second.eventsMask & EPOLLIN) ? aio::etRead : aio::etWrite;
                    triggeredEvent = aio::etError;
                }
                return;
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
        m_impl->selectNextSocket();
        return *this;
    }

    const Socket* PollSet::const_iterator::socket() const
    {
        return static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(m_impl->pollSetImpl->epollEventsArray[m_impl->currentIndex].data.ptr)->first;
    }

    Socket* PollSet::const_iterator::socket()
    {
        return static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(m_impl->pollSetImpl->epollEventsArray[m_impl->currentIndex].data.ptr)->first;
    }


    /*!
        \return bit mask of \a EventType
    */
    aio::EventType PollSet::const_iterator::eventType() const
    {
        return m_impl->triggeredEvent;
    }

    void* PollSet::const_iterator::userData()
    {
        return static_cast<PollSetImpl::MonitoredEventMap::pointer>(m_impl->pollSetImpl->epollEventsArray[m_impl->currentIndex].data.ptr)->second.userData( m_impl->handlerToUse );
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
            //TODO #ak: can this really occur ???
        }
    }

    //!Add socket to set. Does not take socket ownership
    bool PollSet::add( Socket* const sock, EventType eventType, void* userData )
    {
        const int epollEventType = eventType == etRead ? EPOLLIN : EPOLLOUT;

        pair<PollSetImpl::MonitoredEventMap::iterator, bool> p = m_impl->monitoredEvents.insert( make_pair( sock, PollSetImpl::SockData(0, false) ) );
        if( p.second )
        {
            //adding new fd to set
            epoll_event _event;
            memset( &_event, 0, sizeof(_event) );
            _event.data.ptr = &*p.first;
            _event.events = epollEventType | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
            if( epoll_ctl( m_impl->epollSetFD, EPOLL_CTL_ADD, sock->handle(), &_event ) == 0 )
            {
                p.first->second.eventsMask = epollEventType;
                p.first->second.userData(eventType) = userData;
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
            if( p.first->second.eventsMask & epollEventType )
                return true;    //event evenType is already listened on socket

            epoll_event _event;
            memset( &_event, 0, sizeof(_event) );
            _event.data.ptr = &*p.first;
            _event.events = epollEventType | p.first->second.eventsMask | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
            if( epoll_ctl( m_impl->epollSetFD, EPOLL_CTL_MOD, sock->handle(), &_event ) == 0 )
            {
                p.first->second.eventsMask |= epollEventType;
                p.first->second.userData(eventType) = userData;
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    //!Remove socket from set
    void* PollSet::remove( Socket* const sock, EventType eventType )
    {
        const int epollEventType = eventType == etRead ? EPOLLIN : EPOLLOUT;
        PollSetImpl::MonitoredEventMap::iterator it = m_impl->monitoredEvents.find( sock );
        if( (it == m_impl->monitoredEvents.end()) || !(it->second.eventsMask & epollEventType) )
            return NULL;

        const int eventsBesidesRemovedOne = (it->second.eventsMask & (EPOLLIN | EPOLLOUT)) & (~epollEventType);
        if( eventsBesidesRemovedOne )
        {
            //socket is being listened for multiple events, modifying event set
            epoll_event _event;
            memset( &_event, 0, sizeof(_event) );
            _event.data.ptr = &*it;
            _event.events = eventsBesidesRemovedOne | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
            epoll_ctl( m_impl->epollSetFD, EPOLL_CTL_MOD, sock->handle(), &_event );
            it->second.eventsMask &= ~epollEventType;   //resetting disabled event bit
            void* userData = it->second.userData(eventType);
            it->second.userData(eventType) = NULL;
            return userData;
        }
        else
        {
            //socket is being listened for epollEventType only, removing socket...
            epoll_ctl( m_impl->epollSetFD, EPOLL_CTL_DEL, sock->handle(), NULL );
            it->second.markedForRemoval = true;
            for( int i = 0; i < m_impl->signalledSockCount; ++i )
            {
                if( m_impl->epollEventsArray[i].data.ptr != NULL &&
                    static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(m_impl->epollEventsArray[i].data.ptr)->first == sock )
                {
                    m_impl->epollEventsArray[i].data.ptr = NULL;
                    break;
                }
            }
            void* userData = it->second.userData(eventType);
            m_impl->monitoredEvents.erase( it );
            return userData;
        }
    }

    size_t PollSet::size() const
    {
        return m_impl->monitoredEvents.size();
    }

    void* PollSet::getUserData( Socket* const sock, EventType eventType ) const
    {
        const int epollEventType = eventType == etRead ? EPOLLIN : EPOLLOUT;
        PollSetImpl::MonitoredEventMap::iterator it = m_impl->monitoredEvents.find( sock );
        if( (it == m_impl->monitoredEvents.end()) || !(it->second.eventsMask & epollEventType) )
            return NULL;

        return it->second.userData(eventType);
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

    bool PollSet::canAcceptSocket( Socket* const /*sock*/ ) const
    {
        return true;
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
        //TODO #ak: choose proper limit:
            //epoll can accept unlimited descriptors, but total number of available descriptors is limited by system
        return 16*1024;
    }
}

#endif  //__linux__

