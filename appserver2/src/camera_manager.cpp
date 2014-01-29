
#include "camera_manager.h"

#include <functional>

#include <QtConcurrent>

#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "core/resource/camera_resource.h"


namespace ec2
{
    //!This function is intended to perform actions same for every request
    template<class TransactionType,
             class PrepareTransactionFuncType    //functor with no params, returning TransactionType
            >
        ErrorCode doUpdateRequest( PrepareTransactionFuncType prepareTranFunc )
    {
		// todo: const TransactionType& MUST be used. Const absent as temporary solution because we are still using ID instead of GUID

        const TransactionType& tran = prepareTranFunc();
        ErrorCode errorCode = dbManager->executeTransaction( tran );
        if( errorCode != ErrorCode::ok )
            return errorCode;

        // saving transaction to the log
        errorCode = transactionLog->saveTransaction( tran );
        if( errorCode != ErrorCode::ok )
            return errorCode;



        // delivering transaction to remote peers
        return clusterManager->distributeAsync( tran );
    }

    ReqID QnCameraManager::addCamera( const QnVirtualCameraResourcePtr& resource, impl::AddCameraHandlerPtr handler )
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
 

        //declaring output data
        QnVirtualCameraResourceListPtr cameraList;

		ApiCommand command = ec2::updateCamera;
		if (!resource->getId().isValid()) {
			resource->setId(dbManager->getNextSequence());
			command = ec2::addCamera;
		}

        //performing request
        auto prepareTranFunc = std::bind( std::mem_fn( &QnCameraManager::prepareTransaction ), this, command, resource );
        ErrorCode errorCode = doUpdateRequest<QnTransaction<ApiCameraData>, decltype(prepareTranFunc)>( prepareTranFunc );

        //preparing output data
        if( errorCode == ErrorCode::ok )
            cameraList.reset( new QnVirtualCameraResourceList() );

        QtConcurrent::run( std::bind( std::mem_fn( &impl::AddCameraHandler::done ), handler, errorCode, cameraList ) );
        return INVALID_REQ_ID;
    }

    ReqID QnCameraManager::addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnCameraManager::getCameras( QnId mediaServerId, impl::GetCamerasHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnCameraManager::getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnCameraManager::save( const QnVirtualCameraResourceList& cameras, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnCameraManager::remove( const QnVirtualCameraResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    QnTransaction<ApiCameraData> QnCameraManager::prepareTransaction( ec2::ApiCommand cmd, const QnVirtualCameraResourcePtr& resource )
    {
		QnTransaction<ApiCameraData> result;
		result.command = cmd;
		result.createNewID();
		result.persistent = true;
		result.params.fromResource(resource);
        return result;
    }
}
