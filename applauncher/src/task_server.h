////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef TASK_SERVER_H
#define TASK_SERVER_H

#include <memory>

#include <QObject>
#include <QLocalServer>
#include <QSharedPointer>

#include <api/start_application_task.h>
#include "blocking_queue.h"


//!Accepts tasks from application instances and stores them in a task queue
class TaskServer
:
    public QObject
{
    Q_OBJECT

public:
    TaskServer( BlockingQueue<QSharedPointer<applauncher::api::BaseTask> >* const taskQueue );

    //!
    /*!
        Only one server can listen to the pipe (despite \a QLocalServer)
        \return false if failed (e.g., someone uses it) to bind to local address \a pipeName.
    */
    bool listen( const QString& pipeName );

private:
    BlockingQueue<QSharedPointer<applauncher::api::BaseTask> >* const m_taskQueue;
    QLocalServer m_server;

private slots:
    void onNewConnection();
};

#endif  //TASK_SERVER_H
