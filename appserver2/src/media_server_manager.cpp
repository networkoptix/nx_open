
#include "media_server_manager.h"

using namespace ec2;

ReqID QnMediaServerManager::save( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler )
{
    //create transaction
    QnTransaction tran = prepareTransaction( /*...,*/ resource );

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
