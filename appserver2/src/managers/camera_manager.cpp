
#include "camera_manager.h"

#include <functional>

#include <QtConcurrent>

#include "core/resource/camera_resource.h"
#include "database/db_manager.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "transaction/transaction_log.h"
#include "nx_ec/data/api_camera_bookmark_data.h"

namespace ec2
{
    template<class QueryProcessorType>
    QnCameraManager<QueryProcessorType>::QnCameraManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
    :
        QnCameraNotificationManager( resCtx ),
        m_queryProcessor( queryProcessor )
    {
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::addCamera( const QnVirtualCameraResourcePtr& resource, impl::AddCameraHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //preparing output data 
        QnVirtualCameraResourceList cameraList;
        Q_ASSERT_X(
            resource->getId() == QnVirtualCameraResource::uniqueIdToId( resource->getUniqueId() ),
            Q_FUNC_INFO,
            "You must fill camera ID as md5 hash of unique id" );
        if( resource->getId().isNull() )
            resource->setId(QnUuid::createUuid());
        cameraList.push_back( resource );

        //performing request
        auto tran = prepareTransaction( ApiCommand::saveCamera, resource );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::AddCameraHandler::done ), handler, reqID, _1, cameraList ) );
        
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiCommand::Value command = ApiCommand::addCameraHistoryItem;
        auto tran = prepareTransaction( command, cameraHistoryItem );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::removeCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiCommand::Value command = ApiCommand::removeCameraHistoryItem;
        auto tran = prepareTransaction( command, cameraHistoryItem );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getCameras( const QnUuid& mediaServerId, impl::GetCamerasHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiCameraDataList& cameras) {
            QnVirtualCameraResourceList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(cameras, outData, m_resCtx.resFactory);
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<QnUuid, ApiCameraDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getCameras, mediaServerId, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiCameraServerItemDataList& cameraHistory) {
            QnCameraHistoryList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(cameraHistory, outData);
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiCameraServerItemDataList, decltype(queryDoneHandler)> (
            ApiCommand::getCameraHistoryItems, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::save( const QnVirtualCameraResourceList& cameras, impl::AddCameraHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //preparing output data
        QnVirtualCameraResourceList cameraList;
        foreach(const QnVirtualCameraResourcePtr& camera, cameras)
        {
            if (camera->getId().isNull()) {
                Q_ASSERT_X(0, "Only update operation is supported", Q_FUNC_INFO);
                return INVALID_REQ_ID;
            }
            cameraList.push_back( camera );
        }

        //performing request
        auto tran = prepareTransaction( ApiCommand::saveCameras, cameras );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::AddCameraHandler::done ), handler, reqID, _1, cameraList ) );

        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::saveUserAttributes( const QnCameraUserAttributesList& cameraAttributes, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //performing request
        auto tran = prepareTransaction( ApiCommand::saveCameraUserAttributesList, cameraAttributes );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );

        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getUserAttributes( const QnUuid& cameraId, impl::GetCameraUserAttributesHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiCameraAttributesDataList& cameraUserAttributesList ) {
            QnCameraUserAttributesList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(cameraUserAttributesList, outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<QnUuid, ApiCameraAttributesDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getCameraUserAttributes, cameraId, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeCamera, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::addBookmarkTags(const QnCameraBookmarkTags &tags, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::addCameraBookmarkTags, tags );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getBookmarkTags(impl::GetCameraBookmarkTagsHandlerPtr handler)
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiCameraBookmarkTagDataList& tags) {
            QnCameraBookmarkTags outData;
            if (errorCode == ErrorCode::ok)
                for (const ApiCameraBookmarkTagData &tagData: tags)
                    outData << tagData.name;
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiCameraBookmarkTagDataList, decltype(queryDoneHandler)> (
            ApiCommand::getCameraBookmarkTags, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::removeBookmarkTags(const QnCameraBookmarkTags &tags, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeCameraBookmarkTags, tags );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }


    template<class QueryProcessorType>
    QnTransaction<ApiCameraData> QnCameraManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QnVirtualCameraResourcePtr& resource )
    {
        QnTransaction<ApiCameraData> tran(command);
        fromResourceToApi(resource, tran.params);
        return tran;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiCameraDataList> QnCameraManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QnVirtualCameraResourceList& cameras )
    {
        QnTransaction<ApiCameraDataList> tran(command);
        fromResourceListToApi(cameras, tran.params);
        return tran;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiCameraAttributesDataList> QnCameraManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QnCameraUserAttributesList& cameraAttributesList )
    {
        QnTransaction<ApiCameraAttributesDataList> tran(command);
        fromResourceListToApi(cameraAttributesList, tran.params);
        return tran;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiCameraServerItemData> QnCameraManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QnCameraHistoryItem& historyItem )
    {
        QnTransaction<ApiCameraServerItemData> tran(command);
        fromResourceToApi(historyItem, tran.params);
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnCameraManager<T>::prepareTransaction( ApiCommand::Value command, const QnUuid& id )
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }


    template<class T>
    QnTransaction<ApiCameraBookmarkTagDataList> QnCameraManager<T>::prepareTransaction(ApiCommand::Value command, const QnCameraBookmarkTags& tags)
    {
        QnTransaction<ApiCameraBookmarkTagDataList> tran(command);
        fromResourceToApi(tags, tran.params);
        return tran;
    }

    template class QnCameraManager<ServerQueryProcessor>;
    template class QnCameraManager<FixedUrlClientQueryProcessor>;
}
