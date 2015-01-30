/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#include "hybrid_stream_socket.h"

#include <utils/common/systemerror.h>

#include "cloud_tunnel.h"
#include "../system_socket.h"


namespace nx_cc
{
    bool HybridStreamSocket::connect(
        const SocketAddress& remoteAddress,
        unsigned int timeoutMillis )
    {
        //TODO #ak implement by issuing async call and waiting for completion
        return false;
    }

    bool HybridStreamSocket::connectAsyncImpl( const SocketAddress& addr, std::function<void( SystemError::ErrorCode )>&& handler )
    {
        using namespace std::placeholders;
        m_connectHandler = std::move(handler);
        m_remoteAddr = addr;
        std::vector<nx_cc::DnsEntry> dnsEntries;
        if( !nx_cc::DnsTable::instance()->resolveAsync( addr.address, &dnsEntries, std::bind(&HybridStreamSocket::onResolveDone, this, _1) ) )
        {
            //async resolve has been started
            return true;
        }

        if( dnsEntries.empty() )
        {
            m_connectHandler = std::function<void( SystemError::ErrorCode )>();     //resetting handler, since it may hold some resources
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
                return m_socketDelegate->connectAsync( SocketAddress(dnsEntry.address, addr.port), std::move(m_connectHandler) );

            case nx_cc::AddressType::cloud:
            case nx_cc::AddressType::unknown:  //if peer is unknown, trying to establish cloud connect
            {
                //establishing cloud connect
                auto tunnel = CloudTunnelPool::instance()->getTunnelToHost( dnsEntry.address );
                auto connectCompletionHandler = [this]( nx_cc::ErrorDescription errorCode, std::unique_ptr<AbstractStreamSocket> cloudConnection ) { 
                    cloudConnectDone( errorCode, std::move(cloudConnection) );
                };
                if( !tunnel->connect( connectCompletionHandler ) )
                {
                    m_connectHandler = std::function<void( SystemError::ErrorCode )>();     //resetting handler, since it may hold some resources
                    return false;
                }
                return true;
            }

            default:
                assert( false );
                return false;
        }
    }

    void HybridStreamSocket::applyCachedAttributes()
    {
        //TODO #ak applying cached attributes to socket m_socketDelegate
    }

    void HybridStreamSocket::onResolveDone( const std::vector<nx_cc::DnsEntry>& dnsEntries )
    {
        if( dnsEntries.empty() )
        {
            m_connectHandler( SystemError::hostUnreach );
            return;
        }
        //TODO #ak should prefer regular address to a cloud one
        const nx_cc::DnsEntry& dnsEntry = dnsEntries[0];
        switch( dnsEntry.addressType )
        {
            case nx_cc::AddressType::regular:
                //using tcp connection
                m_socketDelegate.reset( new TCPSocket() );
                applyCachedAttributes();
                if( !m_socketDelegate->connectAsync( SocketAddress(dnsEntry.address, m_remoteAddr.port), m_connectHandler ) )
                    m_connectHandler( SystemError::getLastOSErrorCode() );
                return;

            case nx_cc::AddressType::cloud:
            case nx_cc::AddressType::unknown:  //if peer is unknown, trying to establish cloud connect
            {
                //establishing cloud connect
                auto tunnel = CloudTunnelPool::instance()->getTunnelToHost( dnsEntry.address );
                auto connectCompletionHandler = [this]( nx_cc::ErrorDescription errorCode, std::unique_ptr<AbstractStreamSocket> cloudConnection ) { 
                    cloudConnectDone( errorCode, std::move(cloudConnection) );
                };
                if( !tunnel->connect( connectCompletionHandler ) )
                    m_connectHandler( SystemError::getLastOSErrorCode() );
                return;
            }

            default:
                assert( false );
                m_connectHandler( SystemError::hostUnreach );
                return;
        }
    }

    void HybridStreamSocket::cloudConnectDone( nx_cc::ErrorDescription errorCode, std::unique_ptr<AbstractStreamSocket> cloudConnection )
    {
        if( errorCode.resultCode == nx_cc::ResultCode::ok )
        {
            m_socketDelegate = std::move(cloudConnection);
            assert( errorCode.sysErrorCode == SystemError::noError );
        }
        else
        {
            assert( !cloudConnection );
            if( errorCode.sysErrorCode == SystemError::noError )
                errorCode.sysErrorCode = SystemError::hostUnreach;
        }
        auto userHandler = std::move(m_connectHandler);
        userHandler( errorCode.sysErrorCode );  //this object can be freed in handler, so using local variable for handler
    }
}
