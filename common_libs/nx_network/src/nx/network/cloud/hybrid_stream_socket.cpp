#include "hybrid_stream_socket.h"

#include <future>

#include <utils/common/systemerror.h>

#include "cloud_tunnel.h"
#include "../socket_global.h"
#include "../system_socket.h"

namespace nx {
namespace network {
namespace cloud {

bool CloudStreamSocket::connect(
    const SocketAddress& remoteAddress,
    unsigned int timeoutMillis )
{
    //TODO #ak use timeoutMillis

    //issuing async call and waiting for completion
    std::promise<SystemError::ErrorCode> completionPromise;
    auto asyncRequestCompletionResult = completionPromise.get_future();
    auto completionHandler = [&completionPromise]( SystemError::ErrorCode errorCode ) mutable {
        completionPromise.set_value( errorCode );
    };
    connectAsync( remoteAddress, completionHandler );
    asyncRequestCompletionResult.wait();
    if( asyncRequestCompletionResult.get() == SystemError::noError )
        return true;
    SystemError::setLastErrorCode( asyncRequestCompletionResult.get() );
    return false;
}

void CloudStreamSocket::connectAsync(
    const SocketAddress& addr,
    std::function<void( SystemError::ErrorCode )> handler )
{
    //TODO #ak use socket write timeout
    m_connectHandler = std::move(handler);

    const auto remotePort = addr.port;
    nx::network::SocketGlobals::addressResolver().resolveAsync(
        addr.address,
        [this, remotePort](
            SystemError::ErrorCode osErrorCode,
            std::vector<AddressEntry> dnsEntries)
        {
            if (osErrorCode != SystemError::noError)
            {
                auto connectHandlerBak = std::move(m_connectHandler);
                connectHandlerBak(osErrorCode);
                return;
            }

            if (!startAsyncConnect(std::move(dnsEntries), remotePort))
            {
                auto connectHandlerBak = std::move( m_connectHandler );
                connectHandlerBak( SystemError::getLastOSErrorCode() );
            }
        },
        true,
        this);   //TODO #ak resolve cancellation
}

void CloudStreamSocket::applyCachedAttributes()
{
    //TODO #ak applying cached attributes to socket m_socketDelegate
}

void CloudStreamSocket::onResolveDone( std::vector<AddressEntry> dnsEntries )
{
    //TODO #ak which port shell we actually use???
    int port = 0;
    if( !startAsyncConnect( std::move( dnsEntries ), port ) )
    {
        auto connectHandlerBak = std::move( m_connectHandler );
        connectHandlerBak( SystemError::getLastOSErrorCode() );
    }
}

bool CloudStreamSocket::startAsyncConnect(
    std::vector<AddressEntry>&& dnsEntries,
    int port )
{
    if( dnsEntries.empty() )
    {
        SystemError::setLastErrorCode( SystemError::hostUnreach );
        return false;
    }

    //TODO #ak try every resolved address? Also, should prefer regular address to a cloud one
    AddressEntry& dnsEntry = dnsEntries[0];
    switch( dnsEntry.type )
    {
        case AddressType::regular:
            //using tcp connection
            m_socketDelegate.reset( new TCPSocket(true) );
            applyCachedAttributes();
            m_socketDelegate->connectAsync(
                SocketAddress(std::move(dnsEntry.host), port),
                m_connectHandler );
            return true;

        case AddressType::cloud:
        case AddressType::unknown:  //if peer is unknown, trying to establish cloud connect
        {
            unsigned int sockSendTimeout = 0;
            if( !getSendTimeout( &sockSendTimeout ) )
                return false;

            //establishing cloud connect
            auto tunnel = CloudTunnelPool::instance()->getTunnelToHost(
                SocketAddress( std::move( dnsEntry.host ), port ) );
            return tunnel->connect(
                sockSendTimeout,
                [this, tunnel](
                    ErrorDescription errorCode,
                    std::unique_ptr<AbstractStreamSocket> cloudConnection )
                {
                    cloudConnectDone(
                        std::move( tunnel ),
                        errorCode,
                        std::move( cloudConnection ) );
                } );
        }

        default:
            assert( false );
            SystemError::setLastErrorCode( SystemError::hostUnreach );
            return false;
    }
}

void CloudStreamSocket::cloudConnectDone(
    std::shared_ptr<CloudTunnel> /*tunnel*/,
    ErrorDescription errorCode,
    std::unique_ptr<AbstractStreamSocket> cloudConnection )
{
    if( errorCode.resultCode == ResultCode::ok )
    {
        assert( errorCode.sysErrorCode == SystemError::noError );
        m_socketDelegate = std::move(cloudConnection);
        applyCachedAttributes();
    }
    else
    {
        assert( !cloudConnection );
        if( errorCode.sysErrorCode == SystemError::noError )    //e.g., cloud connect is forbidden for target host
            errorCode.sysErrorCode = SystemError::hostUnreach;
    }
    auto userHandler = std::move(m_connectHandler);
    userHandler( errorCode.sysErrorCode );  //this object can be freed in handler, so using local variable for handler
}

} // namespace cloud
} // namespace network
} // namespace nx
