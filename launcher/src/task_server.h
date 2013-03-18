////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef TASK_SERVER_H
#define TASK_SERVER_H

#include <QObject>
#include <QLocalServer>

#include "start_application_task.h"
#include "blocking_queue.h"


//!Accepts tasks from application instances and stores them in a task queue
class TaskServer
:
    public QObject
{
    Q_OBJECT

public:
    TaskServer( BlockingQueue<StartApplicationTask>* const taskQueue );

    //!
    /*!
        Only one server can listen to the pipe (despite \a QLocalServer)
        \return false if failed (e.g., someone uses it) to bind to local address \a pipeName.
    */
    bool listen( const QString& pipeName );

signals:
    //!Emited after task has been posted to the queue
    void taskReceived();

private:
    BlockingQueue<StartApplicationTask>* const m_taskQueue;
    QLocalServer m_server;

private slots:
    void onNewConnection();
};

#endif  //TASK_SERVER_H
