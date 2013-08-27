////////////////////////////////////////////////////////////
// 14 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef TASK_SERVER_NEW_H
#define TASK_SERVER_NEW_H

#include <memory>

#include <QObject>
#include <QSharedPointer>

#include <api/start_application_task.h>
#include <utils/common/long_runnable.h>

#include "blocking_queue.h"
#include "named_pipe_server.h"


//!Accepts tasks from application instances and stores them in a task queue
class TaskServerNew
:
    public QnLongRunnable
{
public:
    TaskServerNew( BlockingQueue<QSharedPointer<applauncher::api::BaseTask> >* const taskQueue );
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
    BlockingQueue<QSharedPointer<applauncher::api::BaseTask> >* const m_taskQueue;
    NamedPipeServer m_server;
    QString m_pipeName;
    bool m_terminated;

    void processNewConnection( NamedPipeSocket* clientConnection );
};

#endif  //TASK_SERVER_NEW_H
