#include "media_server_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{
    QnMediaServerNotificationManager::QnMediaServerNotificationManager()
    {}

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiMediaServerUserAttributesDataList>& tran)
    {
        assert(tran.command == ApiCommand::saveServerUserAttributesList);
        for (const ec2::ApiMediaServerUserAttributesData& attrs: tran.params)
            emit userAttributesChanged(attrs);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiMediaServerUserAttributesData>& tran)
    {
        assert(tran.command == ApiCommand::saveServerUserAttributes);
        emit userAttributesChanged(tran.params);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiIdDataList>& tran)
    {
        assert(tran.command == ApiCommand::removeStorages);
        for (const ApiIdData& idData : tran.params)
            emit storageRemoved(idData.id);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran)
    {
        if (tran.command == ApiCommand::removeMediaServer)
            emit removed(tran.params.id);
        else if (tran.command == ApiCommand::removeStorage)
            emit storageRemoved(tran.params.id);
        else
            Q_ASSERT_X(0, "Invalid transaction", Q_FUNC_INFO);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiStorageDataList>& tran)
    {
        for (const auto& storage : tran.params)
            emit storageChanged(storage);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiStorageData>& tran)
    {
        assert(tran.command == ApiCommand::saveStorage);
        emit storageChanged(tran.params);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiMediaServerData>& tran)
    {
        assert(tran.command == ApiCommand::saveMediaServer);
        emit addedOrUpdated(tran.params);
    }


    template<class QueryProcessorType>
    QnMediaServerManager<QueryProcessorType>::QnMediaServerManager(QueryProcessorType* const queryProcessor)
    :
        QnMediaServerNotificationManager(),
        m_queryProcessor( queryProcessor )
    {}

    template<class T>
    int QnMediaServerManager<T>::getServers(impl::GetServersHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ec2::ApiMediaServerDataList& servers) {
            handler->done( reqID, errorCode, servers);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiMediaServerDataList, decltype(queryDoneHandler)> (
            ApiCommand::getMediaServers, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::save(const ec2::ApiMediaServerData& server, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiMediaServerData> tran(ApiCommand::saveMediaServer, server);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiIdData> tran( ApiCommand::removeMediaServer, id );
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::saveUserAttributes(const ec2::ApiMediaServerUserAttributesDataList& serverAttrs, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiMediaServerUserAttributesDataList> tran(ApiCommand::saveServerUserAttributesList, serverAttrs);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::saveStorages( const ec2::ApiStorageDataList& storages, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnTransaction<ec2::ApiStorageDataList> tran(ApiCommand::saveStorages, storages);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::removeStorages( const ApiIdDataList& storages, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiIdDataList> tran(ApiCommand::removeStorages, storages);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::getUserAttributes( const QnUuid& mediaServerId, impl::GetServerUserAttributesHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiMediaServerUserAttributesDataList& serverUserAttributesList ) {
            handler->done( reqID, errorCode, serverUserAttributesList);
        };
        m_queryProcessor->template processQueryAsync<QnUuid, ApiMediaServerUserAttributesDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getServerUserAttributes, mediaServerId, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::getStorages( const QnUuid& mediaServerId, impl::GetStoragesHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ec2::ApiStorageDataList& storages )
        {
            handler->done( reqID, errorCode, storages );
        };
        m_queryProcessor->template processQueryAsync<QnUuid, ec2::ApiStorageDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getStorages, mediaServerId, queryDoneHandler );
        return reqID;
    }

    template class QnMediaServerManager<ServerQueryProcessor>;
    template class QnMediaServerManager<FixedUrlClientQueryProcessor>;

}
