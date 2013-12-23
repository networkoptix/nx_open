/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef MULTI_ADDRESS_SERVER_H
#define MULTI_ADDRESS_SERVER_H

#include <list>
#include <memory>

#include <utils/common/log.h>
#include <utils/network/socket_common.h>


//!Listens multiple addresses by creating multiple servers (\a SocketServerType)
template<class SocketServerType>
    class MultiAddressServer
{
public:
    MultiAddressServer( const std::list<SocketAddress>& addrToListenList )
    :
        m_addrToListenList( addrToListenList )
    {
    }

    MultiAddressServer( const std::list<SocketAddress>&& addrToListenList )
    :
        m_addrToListenList( addrToListenList )
    {
    }

    /*!
        \param bindToEveryAddress If \a true, this method returns success if bind to every input address succeeded. 
            if \a false - just one successful bind is required for this method to succeed
    */
    bool bind( bool bindToEveryAddress = true )
    {
        //binding to address(-es) to listen
        for( const SocketAddress& addr : m_addrToListenList )
        {
            std::unique_ptr<SocketServerType> socketServer( new SocketServerType() );
            if( !socketServer->bind( addr ) )
            {
                NX_LOG( QString::fromLatin1("Failed to bind to address %1. %2").arg(addr.toString()).arg(SystemError::getLastOSErrorText()), cl_logERROR );
                if( bindToEveryAddress )
                {
                    m_listeners.clear();
                    return false;
                }
                else
                {
                    continue;
                }
            }
            m_listeners.push_back( std::move(socketServer) );
        }

        return !m_listeners.empty();
    }

    //!Returns true, if all binded addresses are successfully listened
    bool listen()
    {
        for( auto it = m_listeners.cbegin(); it != m_listeners.cend(); )
        {
            if( !(*it)->listen() )
            {
                NX_LOG( QString::fromLatin1("Failed to listen address %1. %2").arg((*it)->address().toString()).arg(SystemError::getLastOSErrorText()), cl_logERROR );
                return false;
            }
            else
            {
                ++it;
            }
        }

        return true;
    }

private:
    const std::list<SocketAddress> m_addrToListenList;
    std::list<std::unique_ptr<SocketServerType> > m_listeners;
};

#endif  //MULTI_ADDRESS_SERVER_H
