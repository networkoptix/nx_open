/**********************************************************
* 30 oct 2012
* a.kolesnikov
* PollSet class implementation for win32
***********************************************************/

#ifdef _WIN32

#include "pollset.h"

#include <algorithm>
#include <vector>

#include "../socket.h"


namespace aio
{
    class SocketContext
    {
    public:
        AbstractSocket* socket;
        void* userData;

        SocketContext()
        :
            socket( NULL ),
            userData( NULL )
        {
        }

        SocketContext(
            AbstractSocket* _socket,
            void* _userData = NULL )
        :
            socket( _socket ),
            userData( _userData )
        {
        }

        bool operator<( const SocketContext& right ) const
        {
            return socket < right.socket;
        }

        bool operator<=( const SocketContext& right ) const
        {
            return socket <= right.socket;
        }
    };

    class PollSetImpl
    {
    public:
        //!Elements here are sorted by std::less<>
        std::vector<SocketContext> readSockets;
        //!Elements here are sorted by std::less<>
        std::vector<SocketContext> writeSockets;
        fd_set readfds;
        fd_set writefds;
        fd_set exceptfds;
        std::auto_ptr<AbstractDatagramSocket> dummySocket;

        PollSetImpl()
        {
            FD_ZERO( &readfds );
            FD_ZERO( &writefds );
            FD_ZERO( &exceptfds );

            dummySocket.reset( SocketFactory::createDatagramSocket() );
        }

        void fillFDSet( fd_set* const dest, const std::vector<SocketContext>& src )
        {
            FD_ZERO( dest );
            for( std::vector<SocketContext>::size_type
                i = 0;
                i < src.size();
                ++i )
            {
                FD_SET( src[i].socket->handle(), dest );
            }
        }

        std::vector<SocketContext>::iterator findSocketContextIter(
            AbstractSocket* const sock,
            aio::EventType eventType,
            std::vector<SocketContext>** setToUse )
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
                Q_ASSERT( false );
                return (*setToUse)->end();
            }

            std::vector<SocketContext>::iterator it = std::lower_bound( (*setToUse)->begin(), (*setToUse)->end(), SocketContext(sock), std::less<SocketContext>() );
            if( (it != (*setToUse)->end()) && (it->socket == sock) )
                return it;
            return (*setToUse)->end();
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
            pollSetImpl( NULL ),
            m_nextReadSocketIndex( 0 ),
            m_nextWriteSocketIndex( 0 )
        {
        }

        void findNextSignalledSocket()
        {
            for( ;; )
            {
                currentSocket = SocketContext();
                currentSocketREvent = aio::etNone;
                std::vector<SocketContext>::size_type nextReadSocketIndexBak = m_nextReadSocketIndex;
                //!current index in PollSetImpl::writeSockets
                std::vector<SocketContext>::size_type nextWriteSocketIndexBak = m_nextWriteSocketIndex;
                if( (m_nextReadSocketIndex < pollSetImpl->readSockets.size()) && (m_nextWriteSocketIndex < pollSetImpl->writeSockets.size()) )
                {
                    if( pollSetImpl->readSockets[m_nextReadSocketIndex] <= pollSetImpl->writeSockets[m_nextWriteSocketIndex] )
                    {
                        currentSocket = pollSetImpl->readSockets[m_nextReadSocketIndex];
                        ++m_nextReadSocketIndex;
                    }
                    else if( pollSetImpl->writeSockets[m_nextWriteSocketIndex] < pollSetImpl->readSockets[m_nextReadSocketIndex] )
                    {
                        currentSocket = pollSetImpl->writeSockets[m_nextWriteSocketIndex];
                        ++m_nextWriteSocketIndex;
                    }
                    //else    //pollSetImpl->readSockets[m_nextReadSocketIndex] == pollSetImpl->writeSockets[m_nextWriteSocketIndex]
                    //{
                    //    currentSocket = pollSetImpl->writeSockets[m_nextWriteSocketIndex];
                    //    ++m_nextReadSocketIndex;
                    //    ++m_nextWriteSocketIndex;
                    //}
                }
                else if( m_nextReadSocketIndex < pollSetImpl->readSockets.size() )
                {
                    currentSocket = pollSetImpl->readSockets[m_nextReadSocketIndex];
                    ++m_nextReadSocketIndex;
                }
                else if( m_nextWriteSocketIndex < pollSetImpl->writeSockets.size() )
                {
                    currentSocket = pollSetImpl->writeSockets[m_nextWriteSocketIndex];
                    ++m_nextWriteSocketIndex;
                }

                if( currentSocket.socket )
                {
                    if( m_nextReadSocketIndex != nextReadSocketIndexBak &&
                        FD_ISSET( currentSocket.socket->handle(), &pollSetImpl->readfds ) )
                    {
                        currentSocketREvent = aio::etRead;
                    }
                    if( m_nextWriteSocketIndex != nextWriteSocketIndexBak &&
                        FD_ISSET( currentSocket.socket->handle(), &pollSetImpl->writefds ) )
                    {
                        currentSocketREvent = aio::etWrite;
                    }
                }
                else
                {
                    break;  //reached end()
                }

                if( currentSocket.socket == pollSetImpl->dummySocket.get() )
                    continue;   //skipping dummy socket

                if( currentSocketREvent )
                    break;  //found signalled socket
            }
        }

    private:
        //!current index in PollSetImpl::readSockets
        std::vector<SocketContext>::size_type m_nextReadSocketIndex;
        //!current index in PollSetImpl::writeSockets
        std::vector<SocketContext>::size_type m_nextWriteSocketIndex;
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

    AbstractSocket* PollSet::const_iterator::socket()
    {
        return m_impl->currentSocket.socket;
    }

    const AbstractSocket* PollSet::const_iterator::socket() const
    {
        return m_impl->currentSocket.socket;
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
        return m_impl->currentSocket.userData;
    }

    bool PollSet::const_iterator::operator==( const const_iterator& right ) const
    {
        return (m_impl->pollSetImpl == right.m_impl->pollSetImpl) && (m_impl->currentSocket.socket == right.m_impl->currentSocket.socket);
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
        m_impl->readSockets.push_back( m_impl->dummySocket.get() );
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
        //introduce overlapped IO
        quint8 buf[128];
        m_impl->dummySocket->sendTo( buf, sizeof(buf), m_impl->dummySocket->getLocalAddress() );
    }

    //!Add socket to set. Does not take socket ownership
    bool PollSet::add(
        AbstractSocket* const sock,
        aio::EventType eventType,
        void* userData )
    {
        std::vector<SocketContext>* setToUse = NULL;
        if( eventType == etRead )
        {
            setToUse = &m_impl->readSockets;
        }
        else if( eventType == etWrite )
        {
            setToUse = &m_impl->writeSockets;
        }
        else
        {
            Q_ASSERT( false );
            return false;
        }

        std::vector<SocketContext>::iterator it = std::lower_bound( setToUse->begin(), setToUse->end(), SocketContext(sock), std::less<SocketContext>() );
        if( (it != setToUse->end()) && (it->socket == sock) )
            return true;    //already in set

        if( setToUse->size() == maxPollSetSize() )
            return false;   //no more space

        setToUse->insert( it, SocketContext(sock, userData) );   //adding to position in set
        return true;
    }

    //!Remove socket from set
    void* PollSet::remove( AbstractSocket* const sock, aio::EventType eventType )
    {
        std::vector<SocketContext>* setToUse = NULL;
        std::vector<SocketContext>::iterator it = m_impl->findSocketContextIter( sock, eventType, &setToUse );
        if( it != setToUse->end() )
        {
            void* userData = it->userData;
            setToUse->erase( it );
            return userData;
        }

        return NULL;
    }

    size_t PollSet::size( EventType eventType ) const
    {
        if( eventType == etRead )
            return m_impl->readSockets.size();
        else if( eventType == etWrite )
            return m_impl->writeSockets.size();
        else
        {
            Q_ASSERT( false );
            return maxPollSetSize();
        }
    }

    void* PollSet::getUserData( AbstractSocket* const sock, EventType eventType ) const
    {
        std::vector<SocketContext>* setToUse = NULL;
        std::vector<SocketContext>::iterator it = m_impl->findSocketContextIter( sock, eventType, &setToUse );
        return it != setToUse->end() ? it->userData : NULL;
    }

    /*!
        \param millisToWait if 0, method returns immediatly. If > 0, returns on event or after \a millisToWait milliseconds.
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
        if( (result > 0) && FD_ISSET( m_impl->dummySocket->handle(), &m_impl->readfds ) )
        {
            //reading dummy socket
            quint8 buf[128];
            m_impl->dummySocket->recv( buf, sizeof(buf), 0 );   //ignoring result and data...
             --result;
        }
        return result;
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
        return FD_SETSIZE - 1;  //1 - for dummy socket
    }
}

#endif
