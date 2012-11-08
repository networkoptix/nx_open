/**********************************************************
* 30 oct 2012
* a.kolesnikov
* PollSet class implementation for win32
***********************************************************/

#ifdef _WIN32

#include "pollset.h"

#include <algorithm>
#include <vector>

#include "socket.h"


class PollSetImpl
{
public:
    //!Elements here are sorted by std::less<>
    std::vector<Socket*> readSockets;
    //!Elements here are sorted by std::less<>
    std::vector<Socket*> writeSockets;
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;
    UDPSocket dummySocket;

    PollSetImpl()
    {
        FD_ZERO( &readfds );
        FD_ZERO( &writefds );
        FD_ZERO( &exceptfds );
    }

    void fillFDSet( fd_set* const dest, const std::vector<Socket*>& src )
    {
        FD_ZERO( dest );
        for( std::vector<Socket*>::size_type
            i = 0;
            i < src.size();
            ++i )
        {
            FD_SET( src[i]->handle(), dest );
        }
    }
};

class ConstIteratorImpl
{
public:
    Socket* currentSocket;
    //!bitmask of PollSet::EventType
    unsigned int currentSocketREvents;
    PollSetImpl* pollSetImpl;

    ConstIteratorImpl()
    :
        currentSocket( NULL ),
        currentSocketREvents( 0 ),
        pollSetImpl( NULL ),
        m_nextReadSocketIndex( 0 ),
        m_nextWriteSocketIndex( 0 )
    {
    }

    void findNextSignalledSocket()
    {
        for( ;; )
        {
            currentSocket = NULL;
            currentSocketREvents = 0;
            if( (m_nextReadSocketIndex < pollSetImpl->readSockets.size()) && (m_nextWriteSocketIndex < pollSetImpl->writeSockets.size()) )
            {
                if( pollSetImpl->readSockets[m_nextReadSocketIndex] < pollSetImpl->writeSockets[m_nextWriteSocketIndex] )
                {
                    currentSocket = pollSetImpl->readSockets[m_nextReadSocketIndex];
                    ++m_nextReadSocketIndex;
                }
                else if( pollSetImpl->writeSockets[m_nextWriteSocketIndex] < pollSetImpl->readSockets[m_nextReadSocketIndex] )
                {
                    currentSocket = pollSetImpl->writeSockets[m_nextWriteSocketIndex];
                    ++m_nextWriteSocketIndex;
                }
                else    //pollSetImpl->readSockets[m_nextReadSocketIndex] == pollSetImpl->writeSockets[m_nextWriteSocketIndex]
                {
                    currentSocket = pollSetImpl->writeSockets[m_nextWriteSocketIndex];
                    ++m_nextReadSocketIndex;
                    ++m_nextWriteSocketIndex;
                }
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

            if( currentSocket )
            {
                if( FD_ISSET( currentSocket->handle(), &pollSetImpl->readfds ) )
                    currentSocketREvents |= PollSet::etRead;
                if( FD_ISSET( currentSocket->handle(), &pollSetImpl->writefds ) )
                    currentSocketREvents |= PollSet::etWrite;
            }
            else
            {
                break;  //reached end()
            }

            if( currentSocket == &pollSetImpl->dummySocket )
                continue;   //skipping dummy socket

            if( currentSocketREvents )
                break;  //found signalled socket
        }
    }

private:
    //!current index in PollSetImpl::readSockets
    std::vector<Socket*>::size_type m_nextReadSocketIndex;
    //!current index in PollSetImpl::writeSockets
    std::vector<Socket*>::size_type m_nextWriteSocketIndex;
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

Socket* PollSet::const_iterator::socket() const
{
    return m_impl->currentSocket;
}

/*!
    \return bit mask of \a EventType
*/
unsigned int PollSet::const_iterator::revents() const
{
    return m_impl->currentSocketREvents;
}

bool PollSet::const_iterator::operator==( const const_iterator& right ) const
{
    return (m_impl->pollSetImpl == right.m_impl->pollSetImpl) && (m_impl->currentSocket == right.m_impl->currentSocket);
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
    m_impl->readSockets.push_back( &m_impl->dummySocket );
    m_impl->dummySocket.setNonBlockingMode( true );
}

PollSet::~PollSet()
{
    delete m_impl;
    m_impl = NULL;
}

bool PollSet::isValid() const
{
    return m_impl->dummySocket.handle() > 0;
}

//!Interrupts \a poll method, blocked in other thread
/*!
    This is the only method which is allowed to be called from different thread
*/
void PollSet::interrupt()
{
    //introduce overlapped IO
    quint8 buf[128];
    m_impl->dummySocket.sendTo( buf, sizeof(buf), m_impl->dummySocket.getLocalAddress(), m_impl->dummySocket.getLocalPort() );
}

//!Add socket to set. Does not take socket ownership
bool PollSet::add( Socket* const sock, PollSet::EventType eventType )
{
    std::vector<Socket*>* setToUse = NULL;
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

    std::vector<Socket*>::iterator it = std::lower_bound( setToUse->begin(), setToUse->end(), sock, std::less<Socket*>() );
    if( (it != setToUse->end()) && (*it == sock) )
        return true;    //already in set

    if( setToUse->size() == maxPollSetSize() )
        return false;   //no more space

    setToUse->insert( it, sock );   //adding to position in set
    return true;
}

//!Remove socket from set
void PollSet::remove( Socket* const sock, PollSet::EventType eventType )
{
    std::vector<Socket*>* setToUse = NULL;
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
        return;
    }

    std::vector<Socket*>::iterator it = std::lower_bound( setToUse->begin(), setToUse->end(), sock, std::less<Socket*>() );
    if( (it != setToUse->end()) && (*it == sock) )
    {
        setToUse->erase( it );
        return;
    }
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
    if( (result > 0) && FD_ISSET( m_impl->dummySocket.handle(), &m_impl->readfds ) )
    {
        //reading dummy socket
        quint8 buf[128];
        m_impl->dummySocket.recv( buf, sizeof(buf) );   //ignoring result and data...
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

#endif
