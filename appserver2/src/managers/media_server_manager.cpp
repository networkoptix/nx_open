#include "media_server_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{
    QnMediaServerNotificationManager::QnMediaServerNotificationManager()
    {}

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiMediaServerUserAttributesDataList>& tran, NotificationSource /*source*/)
    {
        NX_ASSERT(tran.command == ApiCommand::saveMediaServerUserAttributesList);
        for (const ec2::ApiMediaServerUserAttributesData& attrs: tran.params)
            emit userAttributesChanged(attrs);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiMediaServerUserAttributesData>& tran, NotificationSource /*source*/)
    {
        NX_ASSERT(tran.command == ApiCommand::saveMediaServerUserAttributes);
        emit userAttributesChanged(tran.params);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiIdDataList>& tran, NotificationSource /*source*/)
    {
        NX_ASSERT(tran.command == ApiCommand::removeStorages);
        for (const ApiIdData& idData : tran.params)
            emit storageRemoved(idData.id);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource /*source*/)
    {
        if (tran.command == ApiCommand::removeMediaServer)
            emit removed(tran.params.id);
        else if (tran.command == ApiCommand::removeStorage)
            emit storageRemoved(tran.params.id);
        else if (tran.command == ApiCommand::removeServerUserAttributes)
            emit userAttributesRemoved(tran.params.id);
        else
            NX_ASSERT(0, "Invalid transaction", Q_FUNC_INFO);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiStorageDataList>& tran, NotificationSource source)
    {
        for (const auto& storage : tran.params)
            emit storageChanged(storage, source);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiStorageData>& tran, NotificationSource source)
    {
        NX_ASSERT(tran.command == ApiCommand::saveStorage);
        emit storageChanged(tran.params, source);
    }

    void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiMediaServerData>& tran, NotificationSource source)
    {
        NX_ASSERT(tran.command == ApiCommand::saveMediaServer);
        emit addedOrUpdated(tran.params, source);
    }


    template<class QueryProcessorType>
    QnMediaServerManager<QueryProcessorType>::QnMediaServerManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData)
    :
      m_queryProcessor( queryProcessor ),
      m_userAccessData(userAccessData)
    {}

    template<class T>
    int QnMediaServerManager<T>::getServers(impl::GetServersHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ec2::ApiMediaServerDataList& servers) {
            handler->done( reqID, errorCode, servers);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiMediaServerDataList, decltype(queryDoneHandler)> (
            ApiCommand::getMediaServers, QnUuid(), queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::getServersEx(impl::GetServersExHandlerPtr handler)
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this](ErrorCode errorCode, const ec2::ApiMediaServerDataExList& servers) {
            handler->done(reqID, errorCode, servers);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiMediaServerDataExList, decltype(queryDoneHandler)>(
            ApiCommand::getMediaServersEx, QnUuid(), queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::save(const ec2::ApiMediaServerData& server, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::saveMediaServer,
            server, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeMediaServer, ApiIdData(id),
            [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::saveUserAttributes(const ec2::ApiMediaServerUserAttributesDataList& serverAttrs, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::saveMediaServerUserAttributesList, serverAttrs,
            [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::saveStorages( const ec2::ApiStorageDataList& storages, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::saveStorages, storages,
            [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::removeStorages( const ApiIdDataList& storages, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeStorages, storages,
            [handler, reqID](ec2::ErrorCode errorCode)
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
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiMediaServerUserAttributesDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getMediaServerUserAttributesList, mediaServerId, queryDoneHandler );
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
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<ParentId, ec2::ApiStorageDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getStorages, mediaServerId, queryDoneHandler );
        return reqID;
    }

    template class QnMediaServerManager<ServerQueryProcessorAccess>;
    template class QnMediaServerManager<FixedUrlClientQueryProcessor>;

}
