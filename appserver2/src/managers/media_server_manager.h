#pragma once

#include <core/resource/resource_fwd.h>

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
        QnMediaServerNotificationManager( const ResourceContext& resCtx );

        void triggerNotification( const QnTransaction<ApiMediaServerData>& tran );

        void triggerNotification( const QnTransaction<ApiStorageData>& tran );

        void triggerNotification( const QnTransaction<ApiStorageDataList>& tran );

        void triggerNotification( const QnTransaction<ApiIdData>& tran );

        void triggerNotification( const QnTransaction<ApiIdDataList>& tran );

        void triggerNotification( const QnTransaction<ApiMediaServerUserAttributesData>& tran );

        void triggerNotification( const QnTransaction<ApiMediaServerUserAttributesDataList>& tran );

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
        virtual int getServers( const QnUuid& mediaServerId,  impl::GetServersHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveServer
        virtual int save( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::remove
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveUserAttributes
        virtual int saveUserAttributes( const QnMediaServerUserAttributesList& serverAttrs, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveStorages
        virtual int saveStorages( const QnStorageResourceList& storages, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::removeStorages
        virtual int removeStorages( const ApiIdDataList& storages, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::getUserAttributes
        virtual int getUserAttributes( const QnUuid& mediaServerId, impl::GetServerUserAttributesHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::getStorages
        virtual int getStorages( const QnUuid& mediaServerId, impl::GetStoragesHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiMediaServerData> prepareTransaction( ApiCommand::Value command, const QnMediaServerResourcePtr& resource );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnUuid& id );
    };
}
