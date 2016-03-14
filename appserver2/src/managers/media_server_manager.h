#pragma once

#include <transaction/transaction.h>

#include <nx_ec/managers/abstract_server_manager.h>

namespace ec2
{
    class QnMediaServerNotificationManager: public AbstractMediaServerManager
    {
    public:
        QnMediaServerNotificationManager();

        void triggerNotification( const QnTransaction<ApiMediaServerData>& tran );

        void triggerNotification( const QnTransaction<ApiStorageData>& tran );

        void triggerNotification( const QnTransaction<ApiStorageDataList>& tran );

        void triggerNotification( const QnTransaction<ApiIdData>& tran );

        void triggerNotification( const QnTransaction<ApiIdDataList>& tran );

        void triggerNotification( const QnTransaction<ApiMediaServerUserAttributesData>& tran );

        void triggerNotification( const QnTransaction<ApiMediaServerUserAttributesDataList>& tran );
    };



    template<class QueryProcessorType>
    class QnMediaServerManager: public QnMediaServerNotificationManager
    {
    public:
        QnMediaServerManager( QueryProcessorType* const queryProcessor);

        //!Implementation of QnMediaServerManager::getServers
        virtual int getServers(impl::GetServersHandlerPtr handler) override;
        //!Implementation of QnMediaServerManager::saveServer
        virtual int save( const ec2::ApiMediaServerData&, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::remove
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveUserAttributes
        virtual int saveUserAttributes( const ec2::ApiMediaServerUserAttributesDataList& serverAttrs, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveStorages
        virtual int saveStorages( const ec2::ApiStorageDataList& storages, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::removeStorages
        virtual int removeStorages( const ApiIdDataList& storages, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::getUserAttributes
        virtual int getUserAttributes( const QnUuid& mediaServerId, impl::GetServerUserAttributesHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::getStorages
        virtual int getStorages( const QnUuid& mediaServerId, impl::GetStoragesHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
    };
}
