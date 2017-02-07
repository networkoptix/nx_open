#pragma once

#include <transaction/transaction.h>

#include <nx_ec/managers/abstract_server_manager.h>

namespace ec2
{
    class QnMediaServerNotificationManager: public AbstractMediaServerNotificationManager
    {
    public:
        QnMediaServerNotificationManager();

        void triggerNotification( const QnTransaction<ApiMediaServerData>& tran, NotificationSource source);

        void triggerNotification( const QnTransaction<ApiStorageData>& tran, NotificationSource source);

        void triggerNotification( const QnTransaction<ApiStorageDataList>& tran, NotificationSource source);

        void triggerNotification( const QnTransaction<ApiIdData>& tran, NotificationSource source);

        void triggerNotification( const QnTransaction<ApiIdDataList>& tran, NotificationSource source);

        void triggerNotification( const QnTransaction<ApiMediaServerUserAttributesData>& tran, NotificationSource source);

        void triggerNotification( const QnTransaction<ApiMediaServerUserAttributesDataList>& tran, NotificationSource source);
    };

    typedef std::shared_ptr<QnMediaServerNotificationManager> QnMediaServerNotificationManagerPtr;

    template<class QueryProcessorType>
    class QnMediaServerManager: public AbstractMediaServerManager
    {
    public:
        QnMediaServerManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);

        //!Implementation of QnMediaServerManager::getServers
        virtual int getServers(impl::GetServersHandlerPtr handler) override;
        virtual int getServersEx(impl::GetServersExHandlerPtr handler) override;
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
        Qn::UserAccessData m_userAccessData;
    };
}
