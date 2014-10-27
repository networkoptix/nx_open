
#ifndef MEDIA_SERVER_MANAGER_H
#define MEDIA_SERVER_MANAGER_H

#include "core/resource/media_server_resource.h"
#include "core/resource/media_server_user_attributes.h"
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
            emit addedOrUpdated( std::move(mserverRes ));
        }

        void triggerNotification( const QnTransaction<ApiStorageData>& tran ) {
            assert( tran.command == ApiCommand::saveStorage);
            
            QnResourceTypePtr resType = m_resCtx.resTypePool->getResourceTypeByName(lit("Storage"));
            if (!resType)
                return;
            
            QnAbstractStorageResourcePtr storage = m_resCtx.resFactory->createResource(resType->getId(), 
                QnResourceParams(tran.params.id, tran.params.url, QString())).dynamicCast<QnAbstractStorageResource>();
            fromApiToResource(tran.params, storage);
            emit storageChanged( std::move(storage) );
        }

        void triggerNotification( const QnTransaction<ApiStorageDataList>& tran )
        {
            QnResourceTypePtr resType = m_resCtx.resTypePool->getResourceTypeByName(lit("Storage"));
            if (!resType)
                return;
            for(const ec2::ApiStorageData& apiStorageData: tran.params) {
                QnAbstractStorageResourcePtr storage = m_resCtx.resFactory->createResource(resType->getId(), 
                    QnResourceParams(apiStorageData.id, apiStorageData.url, QString())).dynamicCast<QnAbstractStorageResource>();
                fromApiToResource(apiStorageData, storage);
                emit storageChanged( std::move(storage) );
            }
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            if( tran.command == ApiCommand::removeMediaServer)
                emit removed( QnUuid(tran.params.id) );
            else if( tran.command == ApiCommand::removeStorage)
                emit storageRemoved( QnUuid(tran.params.id) );
            else
                Q_ASSERT_X(0, "Invalid transaction", Q_FUNC_INFO);
        }

        void triggerNotification( const QnTransaction<ApiIdDataList>& tran )
        {
            if( tran.command == ApiCommand::removeStorages) {
                for(const ApiIdData& idData: tran.params)
                    emit storageRemoved( idData.id );
            }
            else
                Q_ASSERT_X(0, "Invalid transaction", Q_FUNC_INFO);
        }

        void triggerNotification( const QnTransaction<ApiMediaServerUserAttributesData>& tran )
        {
            assert( tran.command == ApiCommand::saveServerUserAttributes );
            QnMediaServerUserAttributesPtr serverAttrs( new QnMediaServerUserAttributes() );
            fromApiToResource( tran.params, serverAttrs );
            emit userAttributesChanged( serverAttrs );
        }

        void triggerNotification( const QnTransaction<ApiMediaServerUserAttributesDataList>& tran )
        {
            assert( tran.command == ApiCommand::saveServerUserAttributesList );
            for(const ApiMediaServerUserAttributesData& attrs: tran.params) 
            {
                QnMediaServerUserAttributesPtr serverAttrs( new QnMediaServerUserAttributes() );
                fromApiToResource( attrs, serverAttrs );
                emit userAttributesChanged( serverAttrs );
            }
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
        virtual int getServers( const QnUuid& mediaServerId,  impl::GetServersHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveServer
        virtual int save( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::remove
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveUserAttributes
        virtual int saveUserAttributes( const QnMediaServerUserAttributesList& serverAttrs, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveStorages
        virtual int saveStorages( const QnAbstractStorageResourceList& storages, impl::SimpleHandlerPtr handler ) override;
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

#endif  //MEDIA_SERVER_MANAGER_H
