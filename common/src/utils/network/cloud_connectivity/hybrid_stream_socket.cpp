/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#include "hybrid_stream_socket.h"

#include <future>

#include <utils/common/systemerror.h>

#include "cloud_tunnel.h"
#include "../system_socket.h"


namespace nx_cc
{
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
        m_remoteAddr = addr;
        std::vector<nx_cc::DnsEntry> dnsEntries;
        const DnsTable::ResolveResult result = DnsTable::instance()->resolveAsync(
            addr.address,
            &dnsEntries,
            std::bind(&CloudStreamSocket::onResolveDone, this, _1) );
        switch( result )
        {
            case DnsTable::ResolveResult::done:
                break;  //operation completed

            case DnsTable::ResolveResult::startedAsync:
                //async resolve has been started
                return true;

            case DnsTable::ResolveResult::failed:
                //resetting handler, since it may hold some resources
                m_connectHandler = std::function<void( SystemError::ErrorCode )>();
                SystemError::setLastErrorCode( SystemError::hostUnreach );
                return false;
        }

        if( startAsyncConnect( std::move( dnsEntries ) ) )
            return true;

        //resetting handler, since it may hold some resources
        m_connectHandler = std::function<void( SystemError::ErrorCode )>();
        return false;
    }

    void CloudStreamSocket::applyCachedAttributes()
    {
        //TODO #ak applying cached attributes to socket m_socketDelegate
    }

    void CloudStreamSocket::onResolveDone( std::vector<nx_cc::DnsEntry> dnsEntries )
    {
        if( !startAsyncConnect( std::move( dnsEntries ) ) )
        {
            auto connectHandlerBak = std::move( m_connectHandler );
            connectHandlerBak( SystemError::getLastOSErrorCode() );
        }
    }

    bool CloudStreamSocket::startAsyncConnect( std::vector<nx_cc::DnsEntry>&& dnsEntries )
    {
        if( dnsEntries.empty() )
        {
            SystemError::setLastErrorCode( SystemError::hostUnreach );
            return false;
        }

        //TODO #ak should prefer regular address to a cloud one
        const nx_cc::DnsEntry& dnsEntry = dnsEntries[0];
        switch( dnsEntry.addressType )
        {
            case nx_cc::AddressType::regular:
                //using tcp connection
                m_socketDelegate.reset( new TCPSocket() );
                applyCachedAttributes();
                return m_socketDelegate->connectAsync(
                    SocketAddress(dnsEntry.address, m_remoteAddr.port),
                    m_connectHandler );

            case nx_cc::AddressType::cloud:
            case nx_cc::AddressType::unknown:  //if peer is unknown, trying to establish cloud connect
            {
                //establishing cloud connect
                auto tunnel = CloudTunnelPool::instance()->getTunnelToHost( dnsEntry.address );
                return tunnel->connect(
                    [this](
                        nx_cc::ErrorDescription errorCode,
                        std::unique_ptr<AbstractStreamSocket> cloudConnection )
                    {
                        cloudConnectDone( errorCode, std::move( cloudConnection ) );
                    } );
            }

            default:
                assert( false );
                SystemError::setLastErrorCode( SystemError::hostUnreach );
                return false;
        }
    }

    void CloudStreamSocket::cloudConnectDone(
        nx_cc::ErrorDescription errorCode,
        std::unique_ptr<AbstractStreamSocket> cloudConnection )
    {
        if( errorCode.resultCode == nx_cc::ResultCode::ok )
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
}
