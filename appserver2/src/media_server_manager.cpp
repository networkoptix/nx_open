
#include "transaction/transaction_log.h"
#include "media_server_manager.h"
#include "database/db_manager.h"
#include "cluster/cluster_manager.h"

using namespace ec2;

QnTransaction<ApiCameraData> prepareTransaction(ApiCommand command, const QnMediaServerResourcePtr& resource)
{
    QnTransaction<ApiCameraData> tran;
    tran.createNewID();
    tran.command = command;
    tran.persistent = true;
    return tran;
}


ReqID QnMediaServerManager::save( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler )
{
    //create transaction
    QnTransaction<ApiCameraData> tran = prepareTransaction( ec2::addCamera, resource );

    // update db
        //get sql query
        //bind params
        //execute
    dbManager->executeTransaction( tran );

    // add to transaction log
    transactionLog->saveTransaction( tran );

    // delivery transaction to the remote peers
    clusterManager->distributeAsync( tran );



    handler->done( ec2::ErrorCode::ok );

    return -1;
}

#if 0

void ClusterNodeListener::transactionReceived( QnTransactionPtr tran )
{
    if( tran.persistent )
    {
        //update db
            //get sql query
            //bind params
            //execute
        DBManager::instance()->executeTransaction( tran );

        //add to transaction log
        TransactionLog::instance()->saveTransaction( tran );
    }

    processRemoteTransaction( tran );
}

void ClusterNodeListener::processRemoteTransaction( const QnTransaction& tran )
{
    //TODO parsing transaction

    //notifying external listener
}

#endif
