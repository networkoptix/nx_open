
#include "camera_manager.h"

#include <functional>

#include <QtConcurrent>

#include "cluster/cluster_manager.h"
#include "core/resource/camera_resource.h"
#include "database/db_manager.h"
#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "transaction/transaction_log.h"


namespace ec2
{
    template<class QueryProcessorType>
    QnCameraManager<QueryProcessorType>::QnCameraManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
    :
        m_queryProcessor( queryProcessor ),
		m_resCtx( resCtx )
    {
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::addCamera( const QnVirtualCameraResourcePtr& resource, impl::AddCameraHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //preparing output data
        QnVirtualCameraResourceList cameraList;
		if (resource->getId().isNull()) {
			resource->setId(QnId::createUuid());
		}
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
    int QnCameraManager<QueryProcessorType>::getCameras( const QnId& mediaServerId, impl::GetCamerasHandlerPtr handler )
    {
        const int reqID = generateRequestID();

		auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiCameraList& cameras) {
			QnVirtualCameraResourceList outData;
			if( errorCode == ErrorCode::ok )
				cameras.toResourceList(outData, m_resCtx.resFactory);
			handler->done( reqID, errorCode, outData);
		};
		m_queryProcessor->template processQueryAsync<QnId, ApiCameraList, decltype(queryDoneHandler)>
			( ApiCommand::getCameras, mediaServerId, queryDoneHandler );
		return reqID;
    }

    template<class QueryProcessorType>
    int QnCameraManager<QueryProcessorType>::getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiCameraServerItemList& cameraHistory) {
            QnCameraHistoryList outData;
            if( errorCode == ErrorCode::ok )
                cameraHistory.toResourceList(outData);
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<nullptr_t, ApiCameraServerItemList, decltype(queryDoneHandler)> (
            ApiCommand::getCameraHistoryList, nullptr, queryDoneHandler );
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
    int QnCameraManager<QueryProcessorType>::remove( const QnId& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeCamera, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }


    template<class QueryProcessorType>
    QnTransaction<ApiCameraData> QnCameraManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QnVirtualCameraResourcePtr& resource )
    {
		QnTransaction<ApiCamera> tran(command, true);
		tran.params.fromResource(resource);
        return tran;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiCameraDataList> QnCameraManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QnVirtualCameraResourceList& cameras )
    {
        QnTransaction<ApiCameraList> tran(command, true);
        tran.params.fromResourceList(cameras);
        return tran;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiCameraServerItemData> QnCameraManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QnCameraHistoryItem& historyItem )
    {
        QnTransaction<ApiCameraServerItem> tran(command, true);
        tran.params.fromResource(historyItem);
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnCameraManager<T>::prepareTransaction( ApiCommand::Value command, const QnId& id )
    {
        QnTransaction<ApiIdData> tran(command, true);
        tran.params.id = id;
        return tran;
    }

    template class QnCameraManager<ServerQueryProcessor>;
    template class QnCameraManager<FixedUrlClientQueryProcessor>;
}
