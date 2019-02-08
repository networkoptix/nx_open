#pragma once

#include <memory>

#include <nx/vms/applauncher/api/applauncher_api.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/ipc/named_pipe_server.h>

class AbstractRequestProcessor;

//!Accepts tasks from application instances and stores them in a task queue
/*!
    Implemented using own implementation of \a NamedPipeServer, since qt's QLocalServer creates win32 pipe only for current user
*/
class TaskServerNew
:
    public QnLongRunnable
{
public:
    TaskServerNew( AbstractRequestProcessor* const requestProcessor );
    virtual ~TaskServerNew();

    //!Implementation of QnLongRunnable::pleaseStop
    virtual void pleaseStop() override;

    //!
    /*!
        Only one server can listen to the pipe (despite \a QLocalServer)
        \return false if failed (e.g., someone uses it) to bind to local address \a pipeName.
    */
    bool listen( const QString& pipeName );

protected:
    virtual void run() override;

private:
    AbstractRequestProcessor* const m_requestProcessor;
    NamedPipeServer m_server;
    QString m_pipeName;
    bool m_terminated;

    void processNewConnection( NamedPipeSocket* clientConnection );
};
