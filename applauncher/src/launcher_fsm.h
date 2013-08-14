////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef LAUNCHER_FSM_H
#define LAUNCHER_FSM_H

#include <memory>

#include <QSettings>
#include <QStateMachine>

#include "installation_manager.h"
#include "launcher_common_data.h"
//#ifdef _WIN32
//#include "task_server_new.h"
//#else
#include "task_server.h"
//#endif
#include "task_queue_watcher.h"


class LauncherFSM
:
    public QStateMachine
{
    Q_OBJECT

    Q_PROPERTY( bool isTaskQueueEmpty READ isTaskQueueEmpty )
    //!Tries to bind to listening named pipe
    Q_PROPERTY( int bindTriesCount READ bindTriesCount )
    //!true, if adding task to the named pipe failed with serverNotFound error
    Q_PROPERTY( bool isLocalServerWasNotFound READ isLocalServerWasNotFound );
    //!true, if application has been launched to kill existing instance
    Q_PROPERTY( bool quitMode READ quitMode );

public:
    LauncherFSM( bool quitMode );

    bool isTaskQueueEmpty() const;
    int bindTriesCount() const;
    bool isLocalServerWasNotFound() const;
    bool quitMode() const;

signals:
    void bindSucceeded();
    void bindFailed();
    void taskAddedToThePipe();
    void failedToAddTaskToThePipe();

private:
    const bool m_quitMode;
    InstallationManager m_installationManager;
    LauncherCommonData m_fsmSharedData;
    BlockingQueue<QSharedPointer<applauncher::api::BaseTask> > m_taskQueue;
//#ifdef _WIN32
//    TaskServerNew m_taskServer;
//#else
    TaskServer m_taskServer;
//#endif
    QSettings m_settings;
    int m_bindTriesCount;
    bool m_isLocalServerWasNotFound;
    TaskQueueWatcher m_taskQueueWatcher;

    void initFSM();

private slots:
    void onBindingToLocalAddressEntered();
    void onAddingTaskToNamedPipeEntered();
    bool addTaskToTheQueue();
    bool getVersionToLaunch( QString* const versionToLaunch, QString* const appArgs );
};

#endif  //LAUNCHER_FSM_H
