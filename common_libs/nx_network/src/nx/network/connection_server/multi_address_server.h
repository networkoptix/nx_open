/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef MULTI_ADDRESS_SERVER_H
#define MULTI_ADDRESS_SERVER_H

#include <list>
#include <memory>
#include <tuple>

#include <common/common_globals.h>
#include <utils/common/cpp14.h>
#include <nx/utils/log/log.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_factory.h>


//!Listens multiple addresses by creating multiple servers (\a SocketServerType)
template<class SocketServerType>
    class MultiAddressServer
{
    template<typename... Args>
    static std::unique_ptr<SocketServerType> realFactoryFunc(Args... params)
    {
        return std::make_unique<SocketServerType>(params...);
    }

public:
    template<typename... Args>
    MultiAddressServer(Args... args)
    {
        //TODO #ak this is work around gcc 4.8 bug. Return to lambda in gcc 4.9
        typedef std::unique_ptr<SocketServerType>(*RealFactoryFuncType)(Args...);
        m_socketServerFactory = 
            std::bind(static_cast<RealFactoryFuncType>(&realFactoryFunc), args...);
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
                const auto osErrorCode = SystemError::getLastOSErrorCode();
                NX_LOG( lit("Failed to bind to address %1. %2").arg(addr.toString()).
                    arg(SystemError::toString(osErrorCode)), cl_logERROR );
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
