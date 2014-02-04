
#include "camera_manager.h"

#include <functional>

#include <QtConcurrent>

#include "cluster/cluster_manager.h"
#include "core/resource/camera_resource.h"
#include "database/db_manager.h"
#include "client_query_processor.h"
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
    ReqID QnCameraManager<QueryProcessorType>::addCamera( const QnVirtualCameraResourcePtr& resource, impl::AddCameraHandlerPtr handler )
    {
        const ReqID reqID = generateRequestID();

        //preparing output data
        QnVirtualCameraResourceList cameraList;
		ApiCommand::Value command = ApiCommand::updateCamera;
		if (!resource->getId().isValid()) {
			resource->setId(dbManager->getNextSequence());
			command = ApiCommand::addCamera;
		}
        cameraList.push_back( resource );

        //performing request
        auto tran = prepareTransaction( command, resource );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::AddCameraHandler::done ), handler, reqID, _1, cameraList ) );
        
        return reqID;
    }

    template<class QueryProcessorType>
    ReqID QnCameraManager<QueryProcessorType>::addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class QueryProcessorType>
    ReqID QnCameraManager<QueryProcessorType>::getCameras( QnId mediaServerId, impl::GetCamerasHandlerPtr handler )
    {
        const ReqID reqID = generateRequestID();

		auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiCameraDataList& cameras) {
			QnVirtualCameraResourceList outData;
			if( errorCode == ErrorCode::ok )
				cameras.toCameraList(outData, m_resCtx.resFactory);
			handler->done( reqID, errorCode, outData);
		};
		m_queryProcessor->processQueryAsync<QnId, ApiCameraDataList, decltype(queryDoneHandler)>
			( ApiCommand::getCameras, mediaServerId, queryDoneHandler );
		return reqID;
    }

    template<class QueryProcessorType>
    ReqID QnCameraManager<QueryProcessorType>::getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler )
    {
        const ReqID reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiCameraServerItemDataList& cameraHistory) {
            QnCameraHistoryList outData;
            if( errorCode == ErrorCode::ok )
                cameraHistory.toResourceList(outData);
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->processQueryAsync<nullptr_t, ApiCameraServerItemDataList, decltype(queryDoneHandler)> (
            ApiCommand::getCameraHistoryList, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class QueryProcessorType>
    ReqID QnCameraManager<QueryProcessorType>::save( const QnVirtualCameraResourceList& cameras, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class QueryProcessorType>
    ReqID QnCameraManager<QueryProcessorType>::remove( const QnVirtualCameraResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiCameraData> QnCameraManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value cmd,
        const QnVirtualCameraResourcePtr& resource )
    {
		QnTransaction<ApiCameraData> result;
		result.command = cmd;
		result.createNewID();
		result.persistent = true;
		result.params.fromResource(resource);
        return result;
    }



    template class QnCameraManager<ServerQueryProcessor>;
    template class QnCameraManager<ClientQueryProcessor>;
}
