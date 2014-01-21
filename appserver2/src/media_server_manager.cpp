
#include "transaction.h"
#include "media_server_manager.h"

using namespace ec2;

QnTransaction<ApiCameraData> prepareTransaction(ApiCommand command, const QnMediaServerResourcePtr& resource)
{
    QnTransaction<ApiCameraData> tran;
    tran.createNewID();
    tran.command = command;
    tran.persistent = true;
    tran.
    return tran;
}


ReqID QnMediaServerManager::save( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler )
{
    //create transaction
    QnTransaction<ApiCameraData> tran = prepareTransaction( addCamera, resource );

    //update db
        //get sql query
        //bind params
        //execute
    DBManager::instance()->executeTransaction( tran );

    //add to transaction log
    TransactionLog::instance()->saveTransaction( tran );

    //
    ClusterManager::instance()->distributeAsync( tran );



    handler->done( ec2::ErrorCode::ok );

    return -1;
}

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
