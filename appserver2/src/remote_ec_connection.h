/**********************************************************
* 30 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef REMOTE_EC_CONNECTION_H
#define REMOTE_EC_CONNECTION_H

#include <memory>

#include <nx_ec/ec_api.h>

#include "base_ec2_connection.h"
#include "client_query_processor.h"
#include "fixed_url_client_query_processor.h"

namespace ec2
{
    class QnTransactionMessageBus;

    class RemoteEC2Connection
    :
        public BaseEc2Connection<FixedUrlClientQueryProcessor>
    {
    public:
        RemoteEC2Connection(
            const FixedUrlClientQueryProcessorPtr& queryProcessor,
            const ResourceContext& resCtx,
            const QnConnectionInfo& connectionInfo );
        virtual ~RemoteEC2Connection();

        virtual QnConnectionInfo connectionInfo() const override;

        template<class T> void processTransaction( const QnTransaction<T>& tran ) {}

        template<> void processTransaction<ApiCameraData>( const QnTransaction<ApiCameraData>& tran ) {
            m_cameraManager->triggerNotification( tran );
        }

        template<> void processTransaction<ApiIdData>( const QnTransaction<ApiIdData>& tran ) {
            switch( tran.command )
            {
                case ApiCommand::removeCamera:
                    return m_cameraManager->triggerNotification( tran );
                case ApiCommand::removeMediaServer:
                    return m_mediaServerManager->triggerNotification( tran );
                case ApiCommand::removeUser:
                    return m_userManager->triggerNotification( tran );
                case ApiCommand::removeBusinessRule:
                    return m_businessEventManager->triggerNotification( tran );
                case ApiCommand::removeLayout:
                    return m_layoutManager->triggerNotification( tran );
                default:
                    assert( false );
            }
        }

        template<> void processTransaction<ApiMediaServerData>( const QnTransaction<ApiMediaServerData>& tran ) {
            m_mediaServerManager->triggerNotification( tran );
        }

        template<> void processTransaction<ApiSetResourceStatusData>( const QnTransaction<ApiSetResourceStatusData>& tran ) {
            m_resourceManager->triggerNotification( tran );
        }

        template<> void processTransaction<ApiResourceParams>( const QnTransaction<ApiResourceParams>& tran ) {
            m_resourceManager->triggerNotification( tran );
        }

        template<> void processTransaction<ApiCameraServerItemData>( const QnTransaction<ApiCameraServerItemData>& tran ) {
            return m_cameraManager->triggerNotification( tran );
        }

        template<> void processTransaction<ApiPanicModeData>( const QnTransaction<ApiPanicModeData>& tran ) {
            //TODO/IMPL
        }

        template<> void processTransaction<QString>( const QnTransaction<QString>& tran ) {
            if( tran.command == ApiCommand::removeStoredFile )
                m_storedFileManager->triggerNotification( tran );
        }

    private:
        FixedUrlClientQueryProcessorPtr m_queryProcessor;
        const QnConnectionInfo m_connectionInfo;
        QnTransactionMessageBus* m_transactionMsg;
    };
}

#endif  //REMOTE_EC_CONNECTION_H
