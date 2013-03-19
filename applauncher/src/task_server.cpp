////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "task_server.h"

#include <QLocalSocket>
#include <QtSingleApplication>

#include <utils/common/log.h>


TaskServer::TaskServer( BlockingQueue<StartApplicationTask>* const taskQueue )
:
    m_taskQueue( taskQueue )
{
}

bool TaskServer::listen( const QString& pipeName )
{
    //on windows multiple m_taskServer can listen single pipe, 
        //on linux unix socket can hang after server crash (because of socket file not removed)
    QtSingleApplication* singleApp = qobject_cast<QtSingleApplication*>(QCoreApplication::instance());
    Q_ASSERT( singleApp );
    if( singleApp->isRunning() )
    {
        NX_LOG( QString::fromLatin1("Application instance already running. Not listening to pipe"), cl_logDEBUG1 );
        return false;
    }

    if( !m_server.listen( pipeName ) )
    {
        NX_LOG( QString::fromLatin1("Failed to listen to pipe %1. %2").arg(pipeName).arg(m_server.errorString()), cl_logDEBUG1 );
        return false;
    }

    NX_LOG( QString::fromLatin1("Listening pipe %1").arg(pipeName), cl_logDEBUG1 );

    connect( &m_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()) );
    return true;
}

void TaskServer::onNewConnection()
{
    std::auto_ptr<QLocalSocket> conn( m_server.nextPendingConnection() );
    if( !conn.get() )
        return;

    if( !conn->waitForReadyRead(-1) )
    {
        NX_LOG( QString::fromLatin1("Failed to receive task data. %1").arg(conn->errorString()), cl_logDEBUG1 );
        return;
    }
    QByteArray data = conn->readAll();
    conn->write( QByteArray("ok") );

    StartApplicationTask task;
    if( !task.deserialize( data ) )
    {
        NX_LOG( QString::fromLatin1("Failed to deserialize received task data"), cl_logDEBUG1 );
        return;
    }

    m_taskQueue->push( task );
    emit taskReceived();
}
