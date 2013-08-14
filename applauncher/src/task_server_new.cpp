////////////////////////////////////////////////////////////
// 14 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "task_server_new.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#include <QtSingleApplication>

#include <utils/common/log.h>

#include "named_pipe_socket.h"


TaskServerNew::TaskServerNew( BlockingQueue<QSharedPointer<applauncher::api::BaseTask> >* const taskQueue )
:
    m_taskQueue( taskQueue ),
    m_terminated( false )
{
}

TaskServerNew::~TaskServerNew()
{
    stop();
}

void TaskServerNew::pleaseStop()
{
    m_terminated = true;
}

bool TaskServerNew::listen( const QString& pipeName )
{
    if( isRunning() )
    {
        pleaseStop();
        wait();
    }

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

    const SystemError::ErrorCode osError = m_server.listen( pipeName );
    if( osError != SystemError::noError )
    {
        NX_LOG( QString::fromLatin1("Failed to listen to pipe %1. %2").arg(pipeName).arg(SystemError::toString(osError)), cl_logDEBUG1 );
        return false;
    }

    m_pipeName = pipeName;

    NX_LOG( QString::fromLatin1("Listening pipe %1").arg(pipeName), cl_logDEBUG1 );

    start();    //launching thread

    return true;
}

static const int ERROR_SLEEP_TIMEOUT_MS = 1000;
static const int MAX_MILLIS_TO_WAIT_CLIENT_CONNECTION = 1000;

void TaskServerNew::run()
{
    while( !m_terminated )
    {
        NamedPipeSocket* clientConnection = NULL;
        const SystemError::ErrorCode osError = m_server.accept( &clientConnection, MAX_MILLIS_TO_WAIT_CLIENT_CONNECTION );
        if( osError != SystemError::noError )
        {
            NX_LOG( QString::fromLatin1("Failed to listen to pipe %1. %2").arg(m_pipeName).arg(SystemError::toString(osError)), cl_logDEBUG1 );
            if( osError != SystemError::timedOut )
                msleep( ERROR_SLEEP_TIMEOUT_MS );
            continue;
        }

        std::auto_ptr<NamedPipeSocket> clientConnectionAp( clientConnection );
        processNewConnection( clientConnection );
    }
}

static const int MAX_MSG_SIZE = 4*1024;

void TaskServerNew::processNewConnection( NamedPipeSocket* clientConnection )
{
    char msgBuf[MAX_MSG_SIZE];

    unsigned int bytesRead = 0;
    SystemError::ErrorCode result = clientConnection->read( msgBuf, MAX_MSG_SIZE, &bytesRead );
    if( result != SystemError::noError )
    {
        NX_LOG( QString::fromLatin1("Failed to receive task data. %1").arg(SystemError::toString(result)), cl_logDEBUG1 );
        return;
    }

    QByteArray responseMsg("ok");
    unsigned int bytesWritten = 0;
    clientConnection->write( responseMsg.constData(), responseMsg.size(), &bytesWritten );
    clientConnection->flush();

    applauncher::api::BaseTask* task = NULL;
    if( !applauncher::api::deserializeTask( QByteArray::fromRawData(msgBuf, bytesRead), &task ) )
    {
        NX_LOG( QString::fromLatin1("Failed to deserialize received task data"), cl_logDEBUG1 );
        return;
    }

#ifdef AK_DEBUG
    if( task->type == applauncher::api::TaskType::startApplication )
        ((applauncher::api::StartApplicationTask*)task)->version = "debug";
#endif

    m_taskQueue->push( QSharedPointer<applauncher::api::BaseTask>(task) );
}
