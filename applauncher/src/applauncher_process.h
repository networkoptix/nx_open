/**********************************************************
* 20 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef APPLAUNCHER_PROCESS_H
#define APPLAUNCHER_PROCESS_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

#include <QSettings>

#include <utils/common/stoppable.h>

#include "abstract_request_processor.h"
#include "installation_manager.h"
#include "rdir_syncher.h"
#ifdef _WIN32
#include "task_server_new.h"
#else
#include "task_server.h"
#endif


class ApplauncherProcess
:
    public QnStoppable,
    public AbstractRequestProcessor,
    public RDirSyncher::EventReceiver
{
public:
    ApplauncherProcess( bool quitMode );

    //!Implementation of \a ApplauncherProcess::pleaseStop()
    virtual void pleaseStop() override;

    //!Implementation of \a AbstractRequestProcessor::processRequest()
    virtual void processRequest(
        const std::shared_ptr<applauncher::api::BaseTask>& request,
        applauncher::api::Response** const response ) override;

    int run();

private:
    bool m_terminated;
    const bool m_quitMode;
    InstallationManager m_installationManager;
#ifdef _WIN32
    TaskServerNew m_taskServer;
#else
    TaskServer m_taskServer;
#endif
    QSettings m_settings;
    int m_bindTriesCount;
    bool m_isLocalServerWasNotFound;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::atomic<int> m_prevDownloadingID;

    bool sendTaskToRunningLauncherInstance();
    bool getVersionToLaunch( QString* const versionToLaunch, QString* const appArgs );
    bool addTaskToThePipe( const QByteArray& serializedTask );
    bool startApplication(
        const std::shared_ptr<applauncher::api::StartApplicationTask>& task,
        applauncher::api::Response* const response );
    bool startInstallation(
        const std::shared_ptr<applauncher::api::StartInstallationTask>& task,
        applauncher::api::InstallResponse* const response );
};

#endif  //APPLAUNCHER_PROCESS_H
