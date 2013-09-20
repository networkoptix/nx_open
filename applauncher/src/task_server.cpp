////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "task_server.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#include <QLocalSocket>
#include <QtSingleApplication>

#include <utils/common/log.h>


TaskServer::TaskServer( BlockingQueue<std::shared_ptr<applauncher::api::BaseTask> >* const taskQueue )
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

#ifndef _WIN32
    //removing unix socket file in case it hanged after process crash
    const QByteArray filePath = (QLatin1String("/tmp/")+pipeName).toLatin1();
    unlink( filePath.constData() );
#endif

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
    conn->flush();

    applauncher::api::BaseTask* task = NULL;
    if( !applauncher::api::deserializeTask( data, &task ) )
    {
        NX_LOG( QString::fromLatin1("Failed to deserialize received task data"), cl_logDEBUG1 );
        return;
    }

#ifdef AK_DEBUG
    if( task->type == applauncher::api::TaskType::startApplication )
        ((applauncher::api::StartApplicationTask*)task)->version = "debug";
#endif

    m_taskQueue->push( std::shared_ptr<applauncher::api::BaseTask>(task) );
}
