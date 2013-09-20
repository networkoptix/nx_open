/**********************************************************
* 20 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef APPLAUNCHER_PROCESS_H
#define APPLAUNCHER_PROCESS_H

#include <memory>

#include <QSettings>

#include <utils/common/stoppable.h>

#include "installation_manager.h"
#ifdef _WIN32
#include "task_server_new.h"
#else
#include "task_server.h"
#endif


class ApplauncherProcess
:
    public QnStoppable
{
public:
    ApplauncherProcess( bool quitMode );

    //!Implementation of \a ApplauncherProcess::pleaseStop()
    virtual void pleaseStop() override;

    int run();

private:
    const bool m_quitMode;
    InstallationManager m_installationManager;
    BlockingQueue<std::shared_ptr<applauncher::api::BaseTask> > m_taskQueue;
#ifdef _WIN32
    TaskServerNew m_taskServer;
#else
    TaskServer m_taskServer;
#endif
    QSettings m_settings;
    int m_bindTriesCount;
    bool m_isLocalServerWasNotFound;

    bool sendTaskToRunningLauncherInstance();
    bool getVersionToLaunch( QString* const versionToLaunch, QString* const appArgs );
    bool addTaskToThePipe( const QByteArray& serializedTask );

    bool startApplication( const std::shared_ptr<applauncher::api::StartApplicationTask>& task );
};

#endif  //APPLAUNCHER_PROCESS_H
