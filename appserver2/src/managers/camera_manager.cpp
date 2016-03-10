#include "camera_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{
    template<class QueryProcessorType>
    QnCameraManager<QueryProcessorType>::QnCameraManager( QueryProcessorType* const queryProcessor )
    :
        QnCameraNotificationManager(),
        m_queryProcessor( queryProcessor )
    {
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::addCamera( const ec2::ApiCameraData& camera, impl::SimpleHandlerPtr handler )
    {
        //TODO: #GDM Move this check to correct place
        /*
        Q_ASSERT_X(
            camera.id == QnVirtualCameraResource::uniqueIdToId( resource->getUniqueId() ),
            Q_FUNC_INFO,
            "You must fill camera ID as md5 hash of unique id" );
        */
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::saveCamera, camera);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::save(const ec2::ApiCameraDataList& cameras, impl::SimpleHandlerPtr handler)
    {
        for (const auto& camera : cameras)
        {
            if (camera.id.isNull())
            {
                Q_ASSERT_X(0, "Only update operation is supported", Q_FUNC_INFO);
                return INVALID_REQ_ID;
            }
        }

        const int reqID = generateRequestID();
        auto tran = prepareTransaction(ApiCommand::saveCameras, cameras);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }


    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::setServerFootageData( const QnUuid& serverGuid, const std::vector<QnUuid>& cameras, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiCommand::Value command = ApiCommand::addCameraHistoryItem;

        QnTransaction<ApiServerFootageData> tran(command);
        tran.params.serverGuid = serverGuid;
        tran.params.archivedCameras = cameras;
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getCameras( const QnUuid& mediaServerId, impl::GetCamerasHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiCameraDataList& cameras)
        {
            handler->done( reqID, errorCode, cameras);
        };
        m_queryProcessor->template processQueryAsync<QnUuid, ApiCameraDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getCameras, mediaServerId, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getServerFootageData( impl::GetCamerasHistoryHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiServerFootageDataList& cameraHistory) {
            ApiServerFootageDataList outData;
            if( errorCode == ErrorCode::ok )
                outData = cameraHistory;
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiServerFootageDataList, decltype(queryDoneHandler)> (
            ApiCommand::getCameraHistoryItems, nullptr, queryDoneHandler );
        return reqID;
    }


    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::saveUserAttributes(const ec2::ApiCameraAttributesDataList& cameraAttributes, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::saveCameraUserAttributesList, cameraAttributes );
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getUserAttributes( const QnUuid& serverId, impl::GetCameraUserAttributesHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiCameraAttributesDataList& cameraUserAttributesList ) {
            handler->done( reqID, errorCode, cameraUserAttributesList);
        };
        m_queryProcessor->template processQueryAsync<QnUuid, ApiCameraAttributesDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getCameraUserAttributes, serverId, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::remove(const QnUuid& id, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeCamera, id );
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiCameraData> QnCameraManager<QueryProcessorType>::prepareTransaction(ApiCommand::Value command, const ec2::ApiCameraData& camera)
    {
        return QnTransaction<ApiCameraData>(command, camera);
    }

    template<class QueryProcessorType>
    QnTransaction<ApiCameraDataList> QnCameraManager<QueryProcessorType>::prepareTransaction(ApiCommand::Value command, const ec2::ApiCameraDataList& cameras)
    {
        return QnTransaction<ApiCameraDataList>(command, cameras);
    }

    template<class QueryProcessorType>
    QnTransaction<ApiCameraAttributesDataList> QnCameraManager<QueryProcessorType>::prepareTransaction(ApiCommand::Value command, const ec2::ApiCameraAttributesDataList& cameraAttributesList )
    {
        return QnTransaction<ApiCameraAttributesDataList>(command, cameraAttributesList);
    }

    template<class T>
    QnTransaction<ApiIdData> QnCameraManager<T>::prepareTransaction( ApiCommand::Value command, const QnUuid& id )
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }

    template class QnCameraManager<ServerQueryProcessor>;
    template class QnCameraManager<FixedUrlClientQueryProcessor>;
}
