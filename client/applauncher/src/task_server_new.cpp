////////////////////////////////////////////////////////////
// 14 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "task_server_new.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#include <qtsinglecoreapplication.h>

#include <nx/utils/log/log.h>

#include "abstract_request_processor.h"


TaskServerNew::TaskServerNew( AbstractRequestProcessor* const requestProcessor )
:
    m_requestProcessor( requestProcessor ),
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
    QtSingleCoreApplication* singleApp = qobject_cast<QtSingleCoreApplication*>(QCoreApplication::instance());
    NX_ASSERT( singleApp );
    if( singleApp->isRunning() )
    {
        NX_DEBUG(this, QString::fromLatin1("Application instance already running. Not listening to pipe"));
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
        NX_DEBUG(this, QString::fromLatin1("Failed to listen to pipe %1. %2").arg(pipeName).arg(SystemError::toString(osError)));
        return false;
    }

    m_pipeName = pipeName;

    NX_DEBUG(this, QString::fromLatin1("Listening pipe %1").arg(pipeName));

    start();    //launching thread

    return true;
}

static const int ERROR_SLEEP_TIMEOUT_MS = 1000;
static const int MAX_MILLIS_TO_WAIT_CLIENT_CONNECTION = 1000;

void TaskServerNew::run()
{
    while (!m_terminated)
    {
        NamedPipeSocket* clientConnection = nullptr;
        const auto osError =
            m_server.accept(&clientConnection, MAX_MILLIS_TO_WAIT_CLIENT_CONNECTION);

        std::unique_ptr<NamedPipeSocket> clientConnectionPtr(clientConnection);

        if (osError != SystemError::noError)
        {
            if (osError == SystemError::timedOut)
                continue;

            NX_DEBUG(this,
                lm("Failed to listen to pipe %1. %2")
                    .arg(m_pipeName).arg(SystemError::toString(osError)));

            msleep(ERROR_SLEEP_TIMEOUT_MS);
            continue;
        }

        processNewConnection(clientConnectionPtr.get());
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
        NX_DEBUG(this, QString::fromLatin1("Failed to receive task data. %1").arg(SystemError::toString(result)));
        return;
    }

    applauncher::api::BaseTask* task = NULL;
    if( !applauncher::api::deserializeTask( QByteArray::fromRawData(msgBuf, bytesRead), &task ) )
    {
        NX_DEBUG(this, QString::fromLatin1("Failed to deserialize received task data"));
        return;
    }

    if( task->type == applauncher::api::TaskType::quit )
        m_terminated = true;

    //m_taskQueue->push( std::shared_ptr<applauncher::api::BaseTask>(task) );
    applauncher::api::Response* response = NULL;
    m_requestProcessor->processRequest(
        std::shared_ptr<applauncher::api::BaseTask>(task),
        &response );

    const QByteArray& responseMsg = response == NULL ? "ok\n\n" : response->serialize();
    unsigned int bytesWritten = 0;
    clientConnection->write( responseMsg.constData(), responseMsg.size(), &bytesWritten );
    clientConnection->flush();
}
