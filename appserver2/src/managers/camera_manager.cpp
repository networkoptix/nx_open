#include "camera_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{
    QnCameraNotificationManager::QnCameraNotificationManager()
    { }

    void QnCameraNotificationManager::triggerNotification(const QnTransaction<ApiCameraData>& tran)
    {
        assert(tran.command == ApiCommand::saveCamera);
        emit addedOrUpdated(tran.params);
    }

    void QnCameraNotificationManager::triggerNotification(const QnTransaction<ApiCameraDataList>& tran)
    {
        assert(tran.command == ApiCommand::saveCameras);
        for (const ApiCameraData& camera : tran.params)
            emit addedOrUpdated(camera);
    }

    void QnCameraNotificationManager::triggerNotification(const QnTransaction<ApiCameraAttributesData>& tran)
    {
        assert(tran.command == ApiCommand::saveCameraUserAttributes);
        emit userAttributesChanged(tran.params);
    }

    void QnCameraNotificationManager::triggerNotification(const QnTransaction<ApiCameraAttributesDataList>& tran)
    {
        assert(tran.command == ApiCommand::saveCameraUserAttributesList);
        for (const ApiCameraAttributesData& attrs : tran.params)
            emit userAttributesChanged(attrs);
    }

    void QnCameraNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran)
    {
        assert(tran.command == ApiCommand::removeCamera);
        emit removed(tran.params.id);
    }

    void QnCameraNotificationManager::triggerNotification(const QnTransaction<ApiServerFootageData>& tran)
    {
        if (tran.command == ApiCommand::addCameraHistoryItem)
            emit cameraHistoryChanged(tran.params);
    }



    template<class QueryProcessorType>
    QnCameraManager<QueryProcessorType>::QnCameraManager( QueryProcessorType* const queryProcessor )
    :
        QnCameraNotificationManager(),
        m_queryProcessor( queryProcessor )
    {
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getCameras(impl::GetCamerasHandlerPtr handler)
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this](ErrorCode errorCode, const ApiCameraDataList& cameras)
        {
            handler->done(reqID, errorCode, cameras);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiCameraDataList, decltype(queryDoneHandler)>
            (ApiCommand::getCameras, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::addCamera( const ec2::ApiCameraData& camera, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiCameraData> tran(ApiCommand::saveCamera, camera);
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
                NX_ASSERT(0, "Only update operation is supported", Q_FUNC_INFO);
                return INVALID_REQ_ID;
            }
        }

        const int reqID = generateRequestID();
        QnTransaction<ApiCameraDataList> tran(ApiCommand::saveCameras, cameras);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::remove(const QnUuid& id, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiIdData> tran(ApiCommand::removeCamera, id);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::setServerFootageData( const QnUuid& serverGuid, const std::vector<QnUuid>& cameras, impl::SimpleHandlerPtr handler )
    {
        ApiServerFootageData data(serverGuid, cameras);
        const int reqID = generateRequestID();
        QnTransaction<ApiServerFootageData> tran(ApiCommand::addCameraHistoryItem, data);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }


    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getServerFootageData( impl::GetCamerasHistoryHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiServerFootageDataList& cameraHistory) {
            handler->done( reqID, errorCode, cameraHistory);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiServerFootageDataList, decltype(queryDoneHandler)> (
            ApiCommand::getCameraHistoryItems, nullptr, queryDoneHandler );
        return reqID;
    }


    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::saveUserAttributes(const ec2::ApiCameraAttributesDataList& cameraAttributes, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        QnTransaction<ApiCameraAttributesDataList> tran(ApiCommand::saveCameraUserAttributesList, cameraAttributes);
        m_queryProcessor->processUpdateAsync(tran, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getUserAttributes(impl::GetCameraUserAttributesHandlerPtr handler)
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiCameraAttributesDataList& cameraUserAttributesList ) {
            handler->done( reqID, errorCode, cameraUserAttributesList);
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiCameraAttributesDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getCameraUserAttributes, nullptr, queryDoneHandler );
        return reqID;
    }

    template class QnCameraManager<ServerQueryProcessor>;
    template class QnCameraManager<FixedUrlClientQueryProcessor>;

}
