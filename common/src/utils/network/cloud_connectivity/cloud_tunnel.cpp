/**********************************************************
* 8 oct 2014
* a.kolesnikov
***********************************************************/

#include "cloud_tunnel.h"


namespace nx_cc
{
    void CloudTunnel::tunnelEstablished( nx_cc::ErrorDescription /*result*/, std::unique_ptr<AbstractStreamSocket> socket )
    {
        //TODO
        m_tunnelSocket = std::move(socket);

        //TODO listening tunnel socket
        //TODO if someone waiting for cloud connection, 
            //if type of tunnel allows us to have multiple connections than creating new connection (if we can)...
            //if type of tunnel does not allow us to have more than one connection, than giving socket to someone and initiating new connection with same(?) acceptor

        processPendingConnectionRequests();
    }

    void CloudTunnel::processPendingConnectionRequests()
    {
        if( m_pendingConnectionRequests.empty() )
            return;

        auto connectionDoneHandler = [this]( nx_cc::ErrorDescription result, std::unique_ptr<AbstractStreamSocket> socket ) {
            assert( !m_pendingConnectionRequests.empty() );
            auto handler = std::move(m_pendingConnectionRequests.front().completionHandler);
            handler( result, std::move(socket) );
            //if someone is waiting for another connection, 
            processPendingConnectionRequests();
        };

        if( !m_tunnelProcessor->createConnection( connectionDoneHandler ) )
        {
            const SystemError::ErrorCode sysErrorCode = SystemError::getLastOSErrorCode();
            //reporting to every who is waiting for connection that there will be no connection
            for( ConnectionRequest& connectionRequest: m_pendingConnectionRequests )
            {
                auto handler = std::move(connectionRequest.completionHandler);
                handler( nx_cc::ErrorDescription( nx_cc::ResultCode::ioError, sysErrorCode ), std::unique_ptr<AbstractStreamSocket>() );
            }
            return;
        }
    }

    std::shared_ptr<CloudTunnel> CloudTunnelPool::getTunnelToHost(
            const HostAddress& targetHost )
    {
        QMutexLocker lock( &m_mutex );
        auto it = m_Pool.find( targetHost );
        if( it == m_Pool.end() )
            it = m_Pool.insert( std::make_pair(
                // TODO: #ak implementation?
                targetHost, std::shared_ptr<CloudTunnel>() ) ).first;

        return it->second;
    }
}
