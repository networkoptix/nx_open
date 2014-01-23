
#include "media_server_manager.h"

#include <functional>

#include <QtConcurrent>

#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"


using namespace ec2;

namespace ec2
{
    ReqID QnMediaServerManager::getServers( impl::GetServersHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnMediaServerManager::save( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        //create transaction
        const QnTransaction<ApiMediaServerData>& tran = prepareTransaction( ec2::saveMediaServer, resource );

        // update db
            //get sql query
            //bind params
            //execute
        dbManager->executeTransaction( tran );

        // add to transaction log
        transactionLog->saveTransaction( tran );

        // delivery transaction to the remote peers
        clusterManager->distributeAsync( tran );

        QtConcurrent::run( std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, ec2::ErrorCode::ok ) );

        return INVALID_REQ_ID;
    }

    ReqID QnMediaServerManager::saveServer( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnMediaServerManager::remove( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    QnTransaction<ApiMediaServerData> QnMediaServerManager::prepareTransaction( ApiCommand command, const QnMediaServerResourcePtr& resource )
    {
        QnTransaction<ApiMediaServerData> tran;
        tran.createNewID();
        tran.command = command;
        tran.persistent = true;
        //TODO/IMPL
        return tran;
    }
}
