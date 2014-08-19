
#ifndef MEDIA_SERVER_MANAGER_H
#define MEDIA_SERVER_MANAGER_H

#include "core/resource/media_server_resource.h"
#include "nx_ec/data/api_media_server_data.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"


namespace ec2
{
    class QnMediaServerNotificationManager
    :
        public AbstractMediaServerManager
    {
    public:
        QnMediaServerNotificationManager( const ResourceContext& resCtx ) : m_resCtx( resCtx ) {}

        void triggerNotification( const QnTransaction<ApiMediaServerData>& tran ) {
            assert( tran.command == ApiCommand::saveMediaServer);
            QnMediaServerResourcePtr mserverRes(new QnMediaServerResource(m_resCtx.resTypePool));
            fromApiToResource(tran.params, mserverRes, m_resCtx);
            emit addedOrUpdated( mserverRes );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeMediaServer );
            emit removed( QUuid(tran.params.id) );
        }

    protected:
        ResourceContext m_resCtx;
    };



    template<class QueryProcessorType>
    class QnMediaServerManager
    :
        public QnMediaServerNotificationManager
    {
    public:
        QnMediaServerManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        //!Implementation of QnMediaServerManager::getServers
        virtual int getServers( impl::GetServersHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveServer
        virtual int save( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::remove
        virtual int remove( const QUuid& id, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiMediaServerData> prepareTransaction( ApiCommand::Value command, const QnMediaServerResourcePtr& resource );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QUuid& id );
    };
}

#endif  //MEDIA_SERVER_MANAGER_H
