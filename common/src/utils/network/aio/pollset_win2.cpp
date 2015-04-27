/**********************************************************
* 16 oct 2014
* a.kolesnikov
* PollSet class implementation for win32
***********************************************************/

#if 1

#ifdef _WIN32

#include "pollset.h"

#include <algorithm>
#include <map>
#include <memory>

#include "../system_socket.h"


namespace aio
{
    static const size_t INITIAL_FDSET_SIZE = 1024;
    static const size_t FDSET_INCREASE_STEP = 1024;

    typedef struct my_fd_set {
        u_int fd_count;               /* how many are SET? */
        //!an array of SOCKETs. Actually, this array has size \a PollSetImpl::fdSetSize
        SOCKET fd_array[INITIAL_FDSET_SIZE];
    } my_fd_set;

    static my_fd_set* allocFDSet( size_t setSize )
    {
        //NOTE: SOCKET is a pointer
        return (my_fd_set*)malloc( sizeof(my_fd_set) + sizeof(SOCKET)*(setSize-INITIAL_FDSET_SIZE) );
    }

    static void freeFDSet( my_fd_set* fdSet )
    {
        ::free( fdSet );
    }

    //!Reallocs \a fdSet to be of size \a newSize while preseving existing data
    /*!
        in case of allocation error \a *fdSet is freed and set to NULL
    */
    static void reallocFDSet( my_fd_set** fdSet, size_t newSize )
    {
        my_fd_set* newSet = allocFDSet( newSize );

        //copying existing data
        if( *fdSet && newSet )
        {
            newSet->fd_count = (*fdSet)->fd_count;
            memcpy( newSet->fd_array, (*fdSet)->fd_array, (*fdSet)->fd_count*sizeof(*((*fdSet)->fd_array)) );
        }

        freeFDSet( *fdSet );
        *fdSet = newSet;
    }

    class PollSetImpl
    {
    public:
        class SockCtx
        {
        public:
            Pollable* sock;
            void* userData[aio::etMax];
            size_t polledEventsMask;

            SockCtx()
            :
                sock( nullptr ),
                polledEventsMask( 0 )
            {
                memset( userData, 0, sizeof(userData) );
            }

            SockCtx(
                Pollable* sock,
                size_t polledEventsMask )
            :
                sock( sock ),
                polledEventsMask( polledEventsMask )
            {
                memset( userData, 0, sizeof(userData) );
            }
        };

        std::map<SOCKET, SockCtx> sockets;
        //!Size of fd sets
        size_t fdSetSize;
        my_fd_set* readfds;
        my_fd_set* writefds;
        my_fd_set* exceptfds;
        //readfdsOriginal (and other *Original) represent fdset before select call
            //These are used for optimization: for easy restoring fdset state after select call
        my_fd_set* readfdsOriginal;
        my_fd_set* writefdsOriginal;
        my_fd_set* exceptfdsOriginal;
        std::unique_ptr<UDPSocket> dummySocket;
        bool modified;

        PollSetImpl()
        :
            fdSetSize( INITIAL_FDSET_SIZE ),
            readfds( allocFDSet( fdSetSize ) ),
            writefds( allocFDSet( fdSetSize ) ),
            exceptfds( allocFDSet( fdSetSize ) ),
            readfdsOriginal( allocFDSet( fdSetSize ) ),
            writefdsOriginal( allocFDSet( fdSetSize ) ),
            exceptfdsOriginal( allocFDSet( fdSetSize ) ),
            modified( true )
        {
            readfds->fd_count = 0;
            writefds->fd_count = 0;
            exceptfds->fd_count = 0;

            readfdsOriginal->fd_count = 0;
            writefdsOriginal->fd_count = 0;
            exceptfdsOriginal->fd_count = 0;

            dummySocket.reset( new UDPSocket() );
        }

        ~PollSetImpl()
        {
            freeFDSet( readfds );
            readfds = NULL;
            freeFDSet( writefds );
            writefds = NULL;
            freeFDSet( exceptfds );
            exceptfds = NULL;

            freeFDSet( readfdsOriginal );
            readfdsOriginal = NULL;
            freeFDSet( writefdsOriginal );
            writefdsOriginal = NULL;
            freeFDSet( exceptfdsOriginal );
            exceptfdsOriginal = NULL;
        }

        void fillFDSets()
        {
            if( modified )
            {
                //pollset has been modified, re-generating fdsets

                readfdsOriginal->fd_count = 0;
                writefdsOriginal->fd_count = 0;
                exceptfdsOriginal->fd_count = 0;

                //expanding arrays if necessary
                if( fdSetSize < sockets.size() )
                {
                    while( fdSetSize < sockets.size() )
                        fdSetSize += FDSET_INCREASE_STEP;

                    reallocFDSet( &readfdsOriginal, fdSetSize );
                    reallocFDSet( &writefdsOriginal, fdSetSize );
                    reallocFDSet( &exceptfdsOriginal, fdSetSize );

                    reallocFDSet( &readfds, fdSetSize );
                    reallocFDSet( &writefds, fdSetSize );
                    reallocFDSet( &exceptfds, fdSetSize );
                }

                for( const std::pair<SOCKET, SockCtx>& val: sockets )
                {
                    if( val.second.polledEventsMask & aio::etRead )
                        readfdsOriginal->fd_array[readfdsOriginal->fd_count++] = val.first;
                    if( val.second.polledEventsMask & aio::etWrite )
                        writefdsOriginal->fd_array[writefdsOriginal->fd_count++] = val.first;
                    exceptfdsOriginal->fd_array[exceptfdsOriginal->fd_count++] = val.first;
                }

                memcpy( readfds->fd_array, readfdsOriginal->fd_array, fdSetSize*sizeof(*readfdsOriginal->fd_array) );
                readfds->fd_count = readfdsOriginal->fd_count;
                memcpy( writefds->fd_array, writefdsOriginal->fd_array, fdSetSize*sizeof(*writefdsOriginal->fd_array) );
                writefds->fd_count = writefdsOriginal->fd_count;
                memcpy( exceptfds->fd_array, exceptfdsOriginal->fd_array, fdSetSize*sizeof(*exceptfdsOriginal->fd_array) );
                exceptfds->fd_count = exceptfdsOriginal->fd_count;

                modified = false;
            }
            else
            {
                //just restoring fdset state after select call
                restoreFDSet( readfds, readfdsOriginal );
                restoreFDSet( writefds, writefdsOriginal );
                restoreFDSet( exceptfds, exceptfdsOriginal );
            }
        }

        void restoreFDSet( my_fd_set* const fds, my_fd_set* const fdsOriginal )
        {
            //select on win32 writes signaled socket handlers to beginning of fd set
            if( fds->fd_count > 0 )
                memcpy( fds->fd_array, fdsOriginal->fd_array, fds->fd_count*sizeof(*fdsOriginal->fd_array) );
            fds->fd_count = fdsOriginal->fd_count;
        }
    };

    class ConstIteratorImpl
    {
    public:
        Pollable* sock;
        void* userData;
        //!bitmask of aio::EventType
        aio::EventType currentSocketREvent;
        PollSetImpl* pollSetImpl;
        my_fd_set* curFdSet;
        //!Index in \a curFdSet.fd_array
        size_t fdIndex;

        ConstIteratorImpl()
        :
            sock( nullptr ),
            userData( nullptr ),
            currentSocketREvent( aio::etNone ),
            pollSetImpl( nullptr ),
            curFdSet( nullptr ),
            fdIndex( 0 )
        {
        }

        void findNextSignalledSocket()
        {
            //moving to the next socket
            if( curFdSet == nullptr )
            {
                curFdSet = pollSetImpl->readfds;
                fdIndex = 0;
            }
            else
            {
                ++fdIndex;
            }

            //win32 select moves signaled sokcet handles to beginning of fd_array and 
                //sets fd_count properly, so it does like epoll

            //NOTE fdIndex points to current next fd. It does not correspond to \a currentSocket and \a currentSocketREvent
            assert( fdIndex <= curFdSet->fd_count );
            //there may be INVALID_SOCKET in fd arrays due to PollSet::remove call
            for( ;; )
            {
                if( fdIndex == curFdSet->fd_count )
                {
                    //taking next fds
                    if( curFdSet == pollSetImpl->readfds )
                        curFdSet = pollSetImpl->writefds;
                    else if( curFdSet == pollSetImpl->writefds )
                        curFdSet = pollSetImpl->exceptfds;
                    else if( curFdSet == pollSetImpl->exceptfds )
                    {
                        sock = nullptr;
                        userData = nullptr;
                        currentSocketREvent = aio::etNone;
                        return; //reached end, iterator invalid
                    }
                    fdIndex = 0;
                    continue;
                }

                if( curFdSet->fd_array[fdIndex] != INVALID_SOCKET )
                    break;  //found valid socket handler
                ++fdIndex;
            }

            //NOTE curFdSet->fd_array[fdIndex] is a valid socket handler

            //TODO #ak remove following find call. Finding socket MUST be constant-time operation
            auto sockIter = pollSetImpl->sockets.find( curFdSet->fd_array[fdIndex] );
            assert( sockIter != pollSetImpl->sockets.end() );

            sock = sockIter->second.sock;
            if( curFdSet == pollSetImpl->readfds )
                currentSocketREvent = aio::etRead;
            else if( curFdSet == pollSetImpl->writefds )
                currentSocketREvent = aio::etWrite;
            else if( curFdSet == pollSetImpl->exceptfds )
                currentSocketREvent = aio::etError;

            //TODO #ak which user data report in case of etError?
            userData = sockIter->second.userData[currentSocketREvent];
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

    PollSet::const_iterator::const_iterator( const PollSet::const_iterator& right )
    :
        m_impl( new ConstIteratorImpl( *right.m_impl ) )
    {
    }

    PollSet::const_iterator::~const_iterator()
    {
        delete m_impl;
        m_impl = NULL;
    }

    PollSet::const_iterator& PollSet::const_iterator::operator=( const PollSet::const_iterator& right )
    {
        *m_impl = *right.m_impl;
        return *this;
    }

    //!Selects next socket which state has been changed with previous \a poll call
    PollSet::const_iterator PollSet::const_iterator::operator++(int)    //it++
    {
        const_iterator bak( *this );
        m_impl->findNextSignalledSocket();
        return bak;
    }

    //!Selects next socket which state has been changed with previous \a poll call
    PollSet::const_iterator& PollSet::const_iterator::operator++()       //++it
    {
        m_impl->findNextSignalledSocket();
        return *this;
    }

    Pollable* PollSet::const_iterator::socket()
    {
        return m_impl->sock;
    }

    const Pollable* PollSet::const_iterator::socket() const
    {
        return m_impl->sock;
    }

    /*!
        \return bit mask of \a EventType
    */
    aio::EventType PollSet::const_iterator::eventType() const
    {
        return m_impl->currentSocketREvent;
    }

    void* PollSet::const_iterator::userData()
    {
        return m_impl->userData;
    }

    bool PollSet::const_iterator::operator==( const const_iterator& right ) const
    {
        assert( m_impl->pollSetImpl == right.m_impl->pollSetImpl );
        return (m_impl->pollSetImpl == right.m_impl->pollSetImpl)
            && (m_impl->curFdSet == right.m_impl->curFdSet)
            && (m_impl->fdIndex == right.m_impl->fdIndex);
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
        m_impl->sockets.emplace(
            m_impl->dummySocket->implementationDelegate()->handle(),
            PollSetImpl::SockCtx( m_impl->dummySocket->implementationDelegate(), aio::etRead ) );
        m_impl->dummySocket->setNonBlockingMode( true );
        m_impl->dummySocket->bind( SocketAddress( HostAddress::localhost, 0 ) );
        
        m_impl->modified = true;
    }

    PollSet::~PollSet()
    {
        delete m_impl;
        m_impl = NULL;
    }

    bool PollSet::isValid() const
    {
        return m_impl->dummySocket->handle() > 0;
    }

    //!Interrupts \a poll method, blocked in other thread
    /*!
        This is the only method which is allowed to be called from different thread
    */
    void PollSet::interrupt()
    {
        //TODO #ak introduce overlapped IO
        quint8 buf[128];
        m_impl->dummySocket->sendTo( buf, sizeof(buf), m_impl->dummySocket->getLocalAddress() );
    }

    //!Add socket to set. Does not take socket ownership
    bool PollSet::add(
        Pollable* const sock,
        aio::EventType eventType,
        void* userData )
    {
        PollSetImpl::SockCtx& ctx = m_impl->sockets[sock->handle()];
        ctx.sock = sock;
        ctx.userData[eventType] = userData;
        ctx.polledEventsMask |= eventType;
        m_impl->modified = true;
        return true;
    }

    //!Remove socket from set
    void PollSet::remove( Pollable* const sock, aio::EventType eventType )
    {
#ifdef _DEBUG
        sock->handle(); //checking that socket object is still alive, since linux and mac implementation use socket in PollSet::remove
#endif

        auto sockIter = m_impl->sockets.find(sock->handle());
        assert( sockIter != m_impl->sockets.end()
            && (sockIter->second.polledEventsMask & eventType) > 0 );    //minor optimization
        if( sockIter == m_impl->sockets.end()
            || (sockIter->second.polledEventsMask & eventType) == 0 )
        {
            //socket is not polled for eventType
            return;
        }

        //removing socket from already occured events list
        if( eventType == aio::etRead )
            std::replace(
                m_impl->readfds->fd_array, m_impl->readfds->fd_array + m_impl->readfds->fd_count,
                sock->handle(), INVALID_SOCKET );
        else if( eventType == aio::etWrite )
            std::replace(
                m_impl->writefds->fd_array, m_impl->writefds->fd_array + m_impl->writefds->fd_count,
                sock->handle(), INVALID_SOCKET );
        std::replace(
            m_impl->exceptfds->fd_array, m_impl->exceptfds->fd_array + m_impl->exceptfds->fd_count,
            sock->handle(), INVALID_SOCKET );

        sockIter->second.polledEventsMask &= ~eventType;
        if( sockIter->second.polledEventsMask == 0 )
            m_impl->sockets.erase( sockIter );
        else
            sockIter->second.userData[eventType] = nullptr;

        m_impl->modified = true;
    }

    size_t PollSet::size() const
    {
        return m_impl->sockets.size();
    }

    /*!
        \param millisToWait if 0, method returns immediately. If > 0, returns on event or after \a millisToWait milliseconds.
            If < 0, method blocks till event
        \return -1 on error, 0 if \a millisToWait timeout has expired of call has been interrupted, > 0 - number of socket whose state has been changed
    */
    int PollSet::poll( int millisToWait )
    {
        m_impl->fillFDSets();

        struct timeval timeout;
        if( millisToWait >= 0 )
        {
            memset( &timeout, 0, sizeof(timeout) );
            timeout.tv_sec = millisToWait / 1000;
            timeout.tv_usec = (millisToWait % 1000) * 1000;
        }
        int result = select( 0, (fd_set*)m_impl->readfds, (fd_set*)m_impl->writefds,
                            (fd_set*)m_impl->exceptfds, millisToWait >= 0 ? &timeout : NULL );
        if( result <= 0 )
            return result;

        for( size_t i = 0; i < m_impl->readfds->fd_count; ++i )
        {
            if( m_impl->readfds->fd_array[i] == m_impl->dummySocket->handle() )
            {
                //reading dummy socket
                quint8 buf[128];
                m_impl->dummySocket->recv( buf, sizeof(buf), 0 );   //ignoring result and data...
                 --result;
                m_impl->readfds->fd_array[i] = INVALID_SOCKET;
                break;
            }
        }

        return result;
    }

    //!Returns iterator pointing to first socket, which state has been changed in previous \a poll call
    PollSet::const_iterator PollSet::begin() const
    {
        const_iterator _begin;
        _begin.m_impl->pollSetImpl = m_impl;
        _begin.m_impl->curFdSet = nullptr;
        _begin.m_impl->findNextSignalledSocket();
        return _begin;
    }

    //!Returns iterator pointing to next element after last socket, which state has been changed in previous \a poll call
    PollSet::const_iterator PollSet::end() const
    {
        const_iterator _end;
        _end.m_impl->pollSetImpl = m_impl;
        _end.m_impl->curFdSet = m_impl->exceptfds;
        _end.m_impl->fdIndex = m_impl->exceptfds->fd_count;
        return _end;
    }
}

#endif

#endif
