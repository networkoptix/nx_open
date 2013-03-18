////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "task_server.h"

#include <QLocalSocket>

#include <utils/common/log.h>


TaskServer::TaskServer( BlockingQueue<StartApplicationTask>* const taskQueue )
:
    m_taskQueue( taskQueue )
{
}

bool TaskServer::listen( const QString& pipeName )
{
    //TODO/IMPL need some semaphore, since on windows multiple m_taskServer can listen single pipe, on linux unix socket will hang in case of server crash

    if( !m_server.listen( pipeName ) )
        return false;

    connect( &m_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()) );
    return true;
}

void TaskServer::onNewConnection()
{
    QLocalSocket* conn = m_server.nextPendingConnection();
    if( !conn )
        return;

    QByteArray data = conn->readAll();
    StartApplicationTask task;
    if( !task.deserialize( data ) )
    {
        NX_LOG( QString::fromLatin1("Failed to deserialize received task data"), cl_logDEBUG1 );
        return;
    }

    m_taskQueue->push( task );
    emit taskReceived();
}
