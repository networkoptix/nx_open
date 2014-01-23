#include "cluster_manager.h"

namespace ec2
{

static QnClusterManager* globalInstance = 0;

QnClusterManager* QnClusterManager::instance()
{
    return globalInstance;
}

void QnClusterManager::initStaticInstance(QnClusterManager* value)
{
    globalInstance = value;
}

template<class T>
void QnClusterManager::transactionReceived( const QnTransaction<T>& tran )
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

template<class T>
void QnClusterManager::processRemoteTransaction( const QnTransaction<T>& tran )
{
    //TODO parsing transaction

    //notifying external listener
}

}
