
#ifndef MEDIA_SERVER_MANAGER_H
#define MEDIA_SERVER_MANAGER_H

#include "core/resource/media_server_resource.h"
#include "nx_ec/data/mserver_data.h"
#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"


namespace ec2
{
    template<class QueryProcessorType>
    class QnMediaServerManager
    :
        public AbstractMediaServerManager
    {
    public:
        QnMediaServerManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        //!Implementation of QnMediaServerManager::getServers
        virtual int getServers( impl::GetServersHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveServer
        virtual int save( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::remove
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) override;

        void triggerNotification( const QnTransaction<ApiMediaServerData>& tran ) {
            assert( tran.command == ApiCommand::saveMediaServer);
            QnMediaServerResourcePtr mserverRes(new QnMediaServerResource(m_resCtx.resTypePool));
            fromApiToResource(tran.params, mserverRes, m_resCtx);
            emit addedOrUpdated( mserverRes );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeMediaServer );
            emit removed( QnId(tran.params.id) );
        }

    private:
        QueryProcessorType* const m_queryProcessor;
        ResourceContext m_resCtx;

        QnTransaction<ApiMediaServerData> prepareTransaction( ApiCommand::Value command, const QnMediaServerResourcePtr& resource );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnId& id );
    };
}

#endif  //MEDIA_SERVER_MANAGER_H
