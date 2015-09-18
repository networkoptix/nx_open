#include "hybrid_stream_socket.h"

#include <future>

#include <utils/common/systemerror.h>

#include "cloud_tunnel.h"
#include "../system_socket.h"

namespace nx {
namespace cc {

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
    if( !connectAsyncImpl( remoteAddress, completionHandler ) )
        return false;
    asyncRequestCompletionResult.wait();
    if( asyncRequestCompletionResult.get() == SystemError::noError )
        return true;
    SystemError::setLastErrorCode( asyncRequestCompletionResult.get() );
    return false;
}

bool CloudStreamSocket::connectAsyncImpl(
    const SocketAddress& addr,
    std::function<void( SystemError::ErrorCode )>&& handler )
{
    //TODO #ak use socket write timeout

    using namespace std::placeholders;
    m_connectHandler = std::move(handler);
    std::vector<AddressEntry> dnsEntries;
    const int remotePort = addr.port;

    return true;
    // TODO: rework later for cloud socket
    /*
    const AddressResolver::ResolveResult result = AddressResolver::instance()->resolveAsync(
        addr.address,
        &dnsEntries,
        [this, remotePort]( std::vector<DnsEntry> dnsEntries )
        {
            if( !startAsyncConnect( std::move( dnsEntries ), remotePort ) )
            {
                auto connectHandlerBak = std::move( m_connectHandler );
                connectHandlerBak( SystemError::getLastOSErrorCode() );
            }
        } );
    switch( result )
    {
        case AddressResolver::ResolveResult::done:
            break;  //operation completed

        case AddressResolver::ResolveResult::startedAsync:
            //async resolve has been started
            return true;

        case AddressResolver::ResolveResult::failed:
            //resetting handler, since it may hold some resources
            m_connectHandler = std::function<void( SystemError::ErrorCode )>();
            SystemError::setLastErrorCode( SystemError::hostUnreach );
            return false;
    }

    if( startAsyncConnect( std::move( dnsEntries ), addr.port ) )
        return true;

    //resetting handler, since it may hold some resources
    m_connectHandler = std::function<void( SystemError::ErrorCode )>();
    return false;
    */

    return true;
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

    return true;
    // TODO: rework
    /*
    //TODO #ak should prefer regular address to a cloud one
    AddressEntry& dnsEntry = dnsEntries[0];
    switch( dnsEntry.type )
    {
        case AddressType::regular:
            //using tcp connection
            m_socketDelegate.reset( new TCPSocket() );
            applyCachedAttributes();
            return m_socketDelegate->connectAsync(
                SocketAddress(std::move(dnsEntry.address), port),
                m_connectHandler );

        case AddressType::cloud:
        case AddressType::unknown:  //if peer is unknown, trying to establish cloud connect
        {
            unsigned int sockSendTimeout = 0;
            if( !getSendTimeout( &sockSendTimeout ) )
                return false;

            //establishing cloud connect
            auto tunnel = CloudTunnelPool::instance()->getTunnelToHost(
                SocketAddress( std::move( dnsEntry.address ), port ) );
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
    */
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

} // namespace cc
} // namespace nx
