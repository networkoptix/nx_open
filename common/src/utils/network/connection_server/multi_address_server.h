/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef MULTI_ADDRESS_SERVER_H
#define MULTI_ADDRESS_SERVER_H

#include <list>
#include <memory>

#include <utils/common/cpp14.h>
#include <utils/common/log.h>
#include <utils/network/socket_common.h>
#include <utils/network/socket_factory.h>


//!Listens multiple addresses by creating multiple servers (\a SocketServerType)
template<class SocketServerType>
    class MultiAddressServer
{
public:
    //TODO #ak introduce genericc implementation when variadic templates available
    template<typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    MultiAddressServer(
        Arg1 arg1,
        Arg2 arg2,
        Arg3 arg3,
        Arg4 arg4 )
    {
        m_socketServerFactory = 
            [arg1, arg2, arg3, arg4]() -> std::unique_ptr<SocketServerType>
            {
                return std::make_unique<SocketServerType>(
                    std::move(arg1),
                    std::move(arg2),
                    std::move(arg3),
                    std::move(arg4));
            };
    }

    template<typename Arg1, typename Arg2>
    MultiAddressServer(
        Arg1 arg1,
        Arg2 arg2 )
    {
        m_socketServerFactory = 
            [arg1, arg2]() -> std::unique_ptr<SocketServerType>
            {
                return std::make_unique<SocketServerType>(
                    std::move(arg1),
                    std::move(arg2));
            };
    }

    /*!
        \param bindToEveryAddress If \a true, this method returns success if bind to every input address succeeded. 
            if \a false - just one successful bind is required for this method to succeed
    */
    bool bind(
        const std::list<SocketAddress>& addrToListenList,
        bool bindToEveryAddress = true )
    {
        //binding to address(-es) to listen
        for( const SocketAddress& addr : addrToListenList )
        {
            std::unique_ptr<SocketServerType> socketServer = m_socketServerFactory();
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
                const auto& errorText = SystemError::getLastOSErrorText();
                NX_LOG( QString::fromLatin1("Failed to listen address %1. %2").arg((*it)->address().toString()).arg(errorText), cl_logERROR );
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
    std::function<std::unique_ptr<SocketServerType>()> m_socketServerFactory;
    std::list<std::unique_ptr<SocketServerType> > m_listeners;
};

#endif  //MULTI_ADDRESS_SERVER_H
