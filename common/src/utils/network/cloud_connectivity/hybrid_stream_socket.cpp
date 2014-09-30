/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#include "hybrid_stream_socket.h"

#include "utils/common/systemerror.h"

#include "cloud_connector.h"
#include "../system_socket.h"


namespace nx_cc
{
    bool HybridStreamSocket::connect(
        const QString& foreignAddress,
        unsigned short foreignPort,
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
                return m_socketDelegate->connectAsync( SocketAddress(dnsEntry.address, addr.port), m_connectHandler );

            case nx_cc::AddressType::cloud:
            case nx_cc::AddressType::unknown:  //if peer is unknown, trying to establish cloud connect
            {
                //establishing cloud connect
                using namespace std::placeholders;
                return CloudConnector::instance()->setupCloudConnection(
                    dnsEntry.address,
                    std::bind(&HybridStreamSocket::cloudConnectDone, this, _1, _2) );
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
                using namespace std::placeholders;
                if( !CloudConnector::instance()->setupCloudConnection(
                        dnsEntry.address,
                        std::bind(&HybridStreamSocket::cloudConnectDone, this, _1, _2) ) )
                    m_connectHandler( SystemError::getLastOSErrorCode() );
                return;
            }

            default:
                assert( false );
                m_connectHandler( SystemError::hostUnreach );
                return;
        }
    }

    void HybridStreamSocket::cloudConnectDone( nx_cc::ErrorDescription errorCode, AbstractStreamSocket* cloudConnection )
    {
        //TODO #ak
    }
}
