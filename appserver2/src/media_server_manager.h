
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
        //!Implementation of QnMediaServerManager::save
        virtual int save( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveServer
        virtual int saveServer( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::remove
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) override;

        template<class T> void triggerNotification( const QnTransaction<T>& tran ) {
            static_assert( false, "Specify QnMediaServerManager::triggerNotification<>, please" );
        }

        template<> void triggerNotification<ApiMediaServerData>( const QnTransaction<ApiMediaServerData>& tran ) {
            assert( tran.command == ApiCommand::addMediaServer || tran.command == ApiCommand::updateMediaServer );
            QnMediaServerResourcePtr mserverRes = m_resCtx.resFactory->createResource(
                tran.params.typeId,
                QnResourceParameters() ).dynamicCast<QnMediaServerResource>();
            tran.params.toResource( mserverRes, m_resCtx.resFactory, m_resCtx.resTypePool );
            emit addedOrUpdated( mserverRes );
        }

        template<> void triggerNotification<ApiIdData>( const QnTransaction<ApiIdData>& tran )
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
