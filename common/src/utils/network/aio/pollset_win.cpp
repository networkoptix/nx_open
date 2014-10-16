/**********************************************************
* 30 oct 2012
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
    typedef std::map<Socket*, void*> PolledSockets;
    typedef std::pair<Socket*, void*> SocketContext;

    class PollSetImpl
    {
    public:
        PolledSockets readSockets;
        PolledSockets writeSockets;
        fd_set readfds;
        fd_set writefds;
        fd_set exceptfds;
        std::unique_ptr<UDPSocket> dummySocket;

        PollSetImpl()
        {
            FD_ZERO( &readfds );
            FD_ZERO( &writefds );
            FD_ZERO( &exceptfds );

            dummySocket.reset( new UDPSocket() );
        }

        void fillFDSet( fd_set* const dest, const PolledSockets& src )
        {
            FD_ZERO( dest );
            const auto endIter = src.cend();
            for( PolledSockets::const_iterator it = src.cbegin(); it != endIter; ++it )
            {
                FD_SET( it->first->handle(), dest );
            }
        }

        PolledSockets::iterator findSocketContextIter(
            Socket* const sock,
            aio::EventType eventType,
            PolledSockets** setToUse )
        {
            *setToUse = &readSockets;   //have to return something in any case
            if( eventType == aio::etRead )
            {
                *setToUse = &readSockets;
            }
            else if( eventType == aio::etWrite )
            {
                *setToUse = &writeSockets;
            }
            else
            {
                assert( false );
                return (*setToUse)->end();
            }

            return (*setToUse)->find( sock );
        }
    };

    class ConstIteratorImpl
    {
    public:
        SocketContext currentSocket;
        //!bitmask of aio::EventType
        aio::EventType currentSocketREvent;
        PollSetImpl* pollSetImpl;

        ConstIteratorImpl()
        :
            currentSocketREvent( aio::etNone ),
            pollSetImpl( nullptr ),
            m_iteratorsAreValid( false )
        {
        }

        void findNextSignalledSocket()
        {
            if( !m_iteratorsAreValid )
            {
                m_nextReadSocketIndex = pollSetImpl->readSockets.cbegin();
                m_nextWriteSocketIndex = pollSetImpl->writeSockets.cbegin();
                m_iteratorsAreValid = true;
            }

            for( ;; )
            {
                currentSocket = SocketContext();
                currentSocketREvent = aio::etNone;
                const auto nextReadSocketIndexBak = m_nextReadSocketIndex;
                //!current index in PollSetImpl::writeSockets
                const auto nextWriteSocketIndexBak = m_nextWriteSocketIndex;
                if( (m_nextReadSocketIndex != pollSetImpl->readSockets.cend()) && (m_nextWriteSocketIndex != pollSetImpl->writeSockets.cend()) )
                {
                    if( m_nextReadSocketIndex->first <= m_nextWriteSocketIndex->first )
                    {
                        currentSocket = *m_nextReadSocketIndex;
                        ++m_nextReadSocketIndex;
                    }
                    else if( m_nextWriteSocketIndex->first < m_nextReadSocketIndex->first )
                    {
                        currentSocket = *m_nextWriteSocketIndex;
                        ++m_nextWriteSocketIndex;
                    }
                    //else    //pollSetImpl->readSockets[m_nextReadSocketIndex] == pollSetImpl->writeSockets[m_nextWriteSocketIndex]
                    //{
                    //    currentSocket = pollSetImpl->writeSockets[m_nextWriteSocketIndex];
                    //    ++m_nextReadSocketIndex;
                    //    ++m_nextWriteSocketIndex;
                    //}
                }
                else if( m_nextReadSocketIndex != pollSetImpl->readSockets.cend() )
                {
                    currentSocket = *m_nextReadSocketIndex;
                    ++m_nextReadSocketIndex;
                }
                else if( m_nextWriteSocketIndex != pollSetImpl->writeSockets.cend() )
                {
                    currentSocket = *m_nextWriteSocketIndex;
                    ++m_nextWriteSocketIndex;
                }

                if( currentSocket.first )
                {
                    if( m_nextReadSocketIndex != nextReadSocketIndexBak &&
                        FD_ISSET( currentSocket.first->handle(), &pollSetImpl->readfds ) )
                    {
                        currentSocketREvent = aio::etRead;
                    }
                    if( m_nextWriteSocketIndex != nextWriteSocketIndexBak &&
                        FD_ISSET( currentSocket.first->handle(), &pollSetImpl->writefds ) )
                    {
                        currentSocketREvent = aio::etWrite;
                    }
                }
                else
                {
                    break;  //reached end()
                }

                if( currentSocket.first == pollSetImpl->dummySocket->implementationDelegate() )
                    continue;   //skipping dummy socket

                if( currentSocketREvent )
                    break;  //found signalled socket
            }
        }

    private:
        bool m_iteratorsAreValid;
        //!current index in PollSetImpl::readSockets
        PolledSockets::const_iterator m_nextReadSocketIndex;
        //!current index in PollSetImpl::writeSockets
        PolledSockets::const_iterator m_nextWriteSocketIndex;
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

    Socket* PollSet::const_iterator::socket()
    {
        return m_impl->currentSocket.first;
    }

    const Socket* PollSet::const_iterator::socket() const
    {
        return m_impl->currentSocket.first;
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
        return m_impl->currentSocket.second;
    }

    bool PollSet::const_iterator::operator==( const const_iterator& right ) const
    {
        return (m_impl->pollSetImpl == right.m_impl->pollSetImpl) && (m_impl->currentSocket.first == right.m_impl->currentSocket.first);
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
        m_impl->readSockets.emplace( m_impl->dummySocket->implementationDelegate(), (void*)nullptr );
        m_impl->dummySocket->setNonBlockingMode( true );
        m_impl->dummySocket->bind( SocketAddress( HostAddress::localhost, 0 ) );
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
        Socket* const sock,
        aio::EventType eventType,
        void* userData )
    {
        PolledSockets* setToUse = NULL;
        if( eventType == etRead )
            setToUse = &m_impl->readSockets;
        else if( eventType == etWrite )
            setToUse = &m_impl->writeSockets;
        else
        {
            assert( false );
            return false;
        }

        //assuming that canAcceptSocket has been called prior to this method
        if( !setToUse->emplace( sock, userData ).second )
            return true;    //socket is already there
        assert( setToUse->size() <= FD_SETSIZE );
        return true;
    }

    //!Remove socket from set
    void* PollSet::remove( Socket* const sock, aio::EventType eventType )
    {
#ifdef _DEBUG
        sock->handle(); //checking that socket object is still alive, since linux and mac implementation use socket in PollSet::remove
#endif

        PolledSockets* setToUse = NULL;
        PolledSockets::iterator it = m_impl->findSocketContextIter( sock, eventType, &setToUse );
        if( it != setToUse->end() )
        {
            void* userData = it->second;
            setToUse->erase( it );
            return userData;
        }

        return nullptr;
    }

    size_t PollSet::size() const
    {
        return std::max<size_t>( m_impl->readSockets.size(), m_impl->writeSockets.size() );
    }

    /*!
        \param millisToWait if 0, method returns immediately. If > 0, returns on event or after \a millisToWait milliseconds.
            If < 0, method blocks till event
        \return -1 on error, 0 if \a millisToWait timeout has expired of call has been interrupted, > 0 - number of socket whose state has been changed
    */
    int PollSet::poll( int millisToWait )
    {
        //filling socket sets
        m_impl->fillFDSet( &m_impl->readfds, m_impl->readSockets );
        m_impl->fillFDSet( &m_impl->writefds, m_impl->writeSockets );
        FD_ZERO( &m_impl->exceptfds );

        struct timeval timeout;
        if( millisToWait >= 0 )
        {
            memset( &timeout, 0, sizeof(timeout) );
            timeout.tv_sec = millisToWait / 1000;
            timeout.tv_usec = (millisToWait % 1000) * 1000;
        }
        int result = select( 0, &m_impl->readfds, &m_impl->writefds, &m_impl->exceptfds, millisToWait >= 0 ? &timeout : NULL );
        if( result > 0 )
            int x = 0;
        if( (result > 0) && FD_ISSET( m_impl->dummySocket->handle(), &m_impl->readfds ) )
        {
            //reading dummy socket
            quint8 buf[128];
            m_impl->dummySocket->recv( buf, sizeof(buf), 0 );   //ignoring result and data...
             --result;
        }
        return result;
    }

    bool PollSet::canAcceptSocket( Socket* const sock ) const
    {
        return
            m_impl->readSockets.find( sock ) != m_impl->readSockets.end() ||
            m_impl->writeSockets.find( sock ) != m_impl->writeSockets.end() ||
            size() < maxPollSetSize();
    }

    //!Returns iterator pointing to first socket, which state has been changed in previous \a poll call
    PollSet::const_iterator PollSet::begin() const
    {
        const_iterator _begin;
        _begin.m_impl->pollSetImpl = m_impl;
        _begin.m_impl->findNextSignalledSocket();
        return _begin;
    }

    //!Returns iterator pointing to next element after last socket, which state has been changed in previous \a poll call
    PollSet::const_iterator PollSet::end() const
    {
        const_iterator _end;
        _end.m_impl->pollSetImpl = m_impl;
        return _end;
    }

    unsigned int PollSet::maxPollSetSize()
    {
        return FD_SETSIZE / 2 - 1;  //1 - for dummy socket, /2 is required to be able to add any socket already present (we MUST always handle every socket in single thread)
    }
}

#endif

#endif
