#include "media_server_manager.h"

#include <functional>

#include <QtConcurrent/QtConcurrent>

#include <core/resource/media_server_resource.h>
#include <core/resource/media_server_user_attributes.h>
#include <core/resource/storage_resource.h>

#include "fixed_url_client_query_processor.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "server_query_processor.h"


using namespace ec2;

namespace ec2
{
    template<class QueryProcessorType>
    QnMediaServerManager<QueryProcessorType>::QnMediaServerManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
    :
        QnMediaServerNotificationManager( resCtx ),
        m_queryProcessor( queryProcessor )
    {
    }

    template<class T>
    int QnMediaServerManager<T>::getServers( const QnUuid& mediaServerId, impl::GetServersHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiMediaServerDataList& servers) {
            QnMediaServerResourceList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(servers, outData, m_resCtx);
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<QnUuid, ApiMediaServerDataList, decltype(queryDoneHandler)> (
            ApiCommand::getMediaServers, mediaServerId, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::save( const QnMediaServerResourcePtr& resource, impl::SaveServerHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        /*
        QnAbstractStorageResourceList storages = resource->getStorages();
        for (int i = 0; i < storages.size(); ++i)
        {
            if (storages[i]->getId().isNull())
                storages[i]->setId(QnUuid::createUuid());
        }
        resource->setStorages(storages);
        */

        //performing request
        auto tran = prepareTransaction( ApiCommand::saveMediaServer, resource );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SaveServerHandler::done, handler, reqID, _1, resource ) );

        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeMediaServer, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::saveUserAttributes( const QnMediaServerUserAttributesList& serverAttrs, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        //performing request
        QnTransaction<ApiMediaServerUserAttributesDataList> tran( ApiCommand::saveServerUserAttributesList );
        fromResourceListToApi(serverAttrs, tran.params);
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, std::placeholders::_1 ) );
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::saveStorages( const QnStorageResourceList& storages, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiStorageDataList> tran(ApiCommand::saveStorages);
        fromResourceToApi(storages, tran.params);
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::removeStorages( const ApiIdDataList& storages, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiIdDataList> tran(ApiCommand::removeStorages, storages);
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::getUserAttributes( const QnUuid& mediaServerId, impl::GetServerUserAttributesHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiMediaServerUserAttributesDataList& serverUserAttributesList ) {
            QnMediaServerUserAttributesList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(serverUserAttributesList, outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<QnUuid, ApiMediaServerUserAttributesDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getServerUserAttributes, mediaServerId, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::getStorages( const QnUuid& mediaServerId, impl::GetStoragesHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiStorageDataList& storages ) 
        {
            QnResourceList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(storages, outData, m_resCtx);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<QnUuid, ApiStorageDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getStorages, mediaServerId, queryDoneHandler );
        return reqID;
    }
    
    template<class T>
    QnTransaction<ApiMediaServerData> QnMediaServerManager<T>::prepareTransaction( ApiCommand::Value command, const QnMediaServerResourcePtr& resource )
    {
        QnTransaction<ApiMediaServerData> tran(command);
        fromResourceToApi(resource, tran.params);
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnMediaServerManager<T>::prepareTransaction( ApiCommand::Value command, const QnUuid& id )
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }


    template class QnMediaServerManager<ServerQueryProcessor>;
    template class QnMediaServerManager<FixedUrlClientQueryProcessor>;

    QnMediaServerNotificationManager::QnMediaServerNotificationManager( const ResourceContext& resCtx ) : m_resCtx( resCtx )
    {

    }

    void QnMediaServerNotificationManager::triggerNotification( const QnTransaction<ApiMediaServerUserAttributesDataList>& tran )
    {
        assert( tran.command == ApiCommand::saveServerUserAttributesList );
        for(const ApiMediaServerUserAttributesData& attrs: tran.params) 
        {
            QnMediaServerUserAttributesPtr serverAttrs( new QnMediaServerUserAttributes() );
            fromApiToResource( attrs, serverAttrs );
            emit userAttributesChanged( serverAttrs );
        }
    }

    void QnMediaServerNotificationManager::triggerNotification( const QnTransaction<ApiMediaServerUserAttributesData>& tran )
    {
        assert( tran.command == ApiCommand::saveServerUserAttributes );
        QnMediaServerUserAttributesPtr serverAttrs( new QnMediaServerUserAttributes() );
        fromApiToResource( tran.params, serverAttrs );
        emit userAttributesChanged( serverAttrs );
    }

    void QnMediaServerNotificationManager::triggerNotification( const QnTransaction<ApiIdDataList>& tran )
    {
        if( tran.command == ApiCommand::removeStorages) {
            for(const ApiIdData& idData: tran.params)
                emit storageRemoved( idData.id );
        }
        else
            Q_ASSERT_X(0, "Invalid transaction", Q_FUNC_INFO);
    }

    void QnMediaServerNotificationManager::triggerNotification( const QnTransaction<ApiIdData>& tran )
    {
        if( tran.command == ApiCommand::removeMediaServer)
            emit removed( QnUuid(tran.params.id) );
        else if( tran.command == ApiCommand::removeStorage)
            emit storageRemoved( QnUuid(tran.params.id) );
        else
            Q_ASSERT_X(0, "Invalid transaction", Q_FUNC_INFO);
    }

    void QnMediaServerNotificationManager::triggerNotification( const QnTransaction<ApiStorageDataList>& tran )
    {
        QnResourceTypePtr resType = m_resCtx.resTypePool->getResourceTypeByName(lit("Storage"));
        if (!resType)
            return;
        for(const ec2::ApiStorageData& apiStorageData: tran.params) {
            QnStorageResourcePtr storage = m_resCtx.resFactory->createResource(resType->getId(), 
                QnResourceParams(apiStorageData.id, apiStorageData.url, QString())).dynamicCast<QnStorageResource>();
            fromApiToResource(apiStorageData, storage);
            emit storageChanged( std::move(storage) );
        }
    }

    void QnMediaServerNotificationManager::triggerNotification( const QnTransaction<ApiStorageData>& tran )
    {
        assert( tran.command == ApiCommand::saveStorage);

        QnResourceTypePtr resType = m_resCtx.resTypePool->getResourceTypeByName(lit("Storage"));
        if (!resType)
            return;

        QnStorageResourcePtr storage = m_resCtx.resFactory->createResource(resType->getId(), 
            QnResourceParams(tran.params.id, tran.params.url, QString())).dynamicCast<QnStorageResource>();
        fromApiToResource(tran.params, storage);
        emit storageChanged( std::move(storage) );
    }

    void QnMediaServerNotificationManager::triggerNotification( const QnTransaction<ApiMediaServerData>& tran )
    {
        assert( tran.command == ApiCommand::saveMediaServer);
        QnMediaServerResourcePtr mserverRes(new QnMediaServerResource(m_resCtx.resTypePool));
        fromApiToResource(tran.params, mserverRes, m_resCtx);
        emit addedOrUpdated( std::move(mserverRes ));
    }



}
