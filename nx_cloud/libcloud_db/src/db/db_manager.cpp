/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "db_manager.h"

#include <thread>

#include <utils/common/log.h>

#include "db_structure_updater.h"


namespace cdb {
namespace db {


static const int WAITING_TASKS_COEFF = 5;

DBManager::DBManager( const ConnectionOptions& connectionOptions )
:
    m_connectionOptions( connectionOptions ),
    m_connectionsBeingAdded( 0 )
{
}

DBManager::~DBManager()
{
}

bool DBManager::init()
{
    if( !openOneMoreConnectionIfNeeded() )
        return false;

    //updating DB structure to actual state
    DBStructureUpdater dbStructureUpdater( this );
    return dbStructureUpdater.updateStructSync();
}

bool DBManager::openOneMoreConnectionIfNeeded()
{
    {
        QnMutexLocker lk( &m_mutex );   

        const auto effectiveDBConnectionCount = m_dbThreadPool.size() + m_connectionsBeingAdded;
        if( m_requestQueue.size() < effectiveDBConnectionCount * WAITING_TASKS_COEFF ||  //task number is not too high
            effectiveDBConnectionCount >= m_connectionOptions.maxConnectionCount )    //pool size is already at maximum
        {
            return true;
        }
        ++m_connectionsBeingAdded;
    }

    //adding another connection
    auto executorThread = std::make_unique<DbRequestExecutionThread>(
        m_connectionOptions,
        &m_requestQueue );
    //avoiding locking mutex for connecting phase
    if( !executorThread->open() )
    {
        NX_LOG( lit("Failed to initialize connection to DB"), cl_logWARNING );
        QnMutexLocker lk( &m_mutex );
        --m_connectionsBeingAdded;
        return false;
    }
    executorThread->start();

    QnMutexLocker lk( &m_mutex );
    m_dbThreadPool.push_back( std::move( executorThread ) );
    --m_connectionsBeingAdded;
    return true;
}


}   //db
}   //cdb
