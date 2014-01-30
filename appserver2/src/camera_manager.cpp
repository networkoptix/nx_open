
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
    QnCameraManager<QueryProcessorType>::QnCameraManager( QueryProcessorType* const queryProcessor, QSharedPointer<QnResourceFactory> factory)
    :
        m_queryProcessor( queryProcessor ),
		m_resourceFactory( factory )
    {
    }

    template<class QueryProcessorType>
    ReqID QnCameraManager<QueryProcessorType>::addCamera( const QnVirtualCameraResourcePtr& resource, impl::AddCameraHandlerPtr handler )
    {
#if 0
        //building async pipe
        const QnTransaction<ApiCameraData>& tran = prepareTransaction( ec2::addCamera, resource );

        AsyncPipeline pipeline;
        pipeline.addSyncCall( std::bind( std::mem_fn(&QnDbManager::executeTransaction), dbManager, tran ) );
        pipeline.addSyncCall( std::bind( std::mem_fn(&QnTransactionLog::saveTransaction), transactionLog, tran ) );
        pipeline.addAsyncCall( std::bind( std::mem_fn(&QnClusterManager::distributeAsync), clusterManager, tran ) );
        //preparing output data
        pipeline.addSyncCall( []() {
            cameraList.reset( new QnVirtualCameraResourceList() );
            return ErrorCode::ok; } );
        pipeline.addSyncCall( std::bind( std::mem_fn(&impl::AddCameraHandler::done), handler, ErrorCode::ok, cameraList ) );
        AsyncPipelineExecuter::instance()->run(pipeline);

        return INVALID_REQ_ID;
#endif

        //preparing output data
        QnVirtualCameraResourceListPtr cameraList( new QnVirtualCameraResourceList() );
		ApiCommand command = ec2::updateCamera;
		if (!resource->getId().isValid()) {
			resource->setId(dbManager->getNextSequence());
			command = ec2::addCamera;
		}
        cameraList->push_back( resource );

        //performing request
        auto tran = prepareTransaction( command, resource );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::AddCameraHandler::done ), handler, _1, cameraList ) );
        
        return INVALID_REQ_ID;
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
		auto queryDoneHandler = [handler, this]( ErrorCode errorCode, const ApiCameraDataList& cameras) {
			QnVirtualCameraResourceListPtr outData(new QnVirtualCameraResourceList());
			if( errorCode == ErrorCode::ok )
				cameras.toCameraList(*outData.data(), m_resourceFactory.data());
			handler->done( errorCode, outData);
		};
		QnResourceParameters params;
		if (mediaServerId.isValid())
			params["serverId"] = mediaServerId.toString();
		m_queryProcessor->processQueryAsync<QnResourceParameters, ApiCameraDataList, decltype(queryDoneHandler)>
			( params, queryDoneHandler );
		return INVALID_REQ_ID;
    }

    template<class QueryProcessorType>
    ReqID QnCameraManager<QueryProcessorType>::getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
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
    QnTransaction<ApiCameraData> QnCameraManager<QueryProcessorType>::prepareTransaction( ec2::ApiCommand cmd, const QnVirtualCameraResourcePtr& resource )
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
