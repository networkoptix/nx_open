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

#include <QObject>
#include <QSettings>

#include <utils/common/stoppable.h>
#include <utils/common/timermanager.h>

#include "abstract_request_processor.h"
#include "installation_manager.h"
#include "installation_process.h"
#include "task_server_new.h"
#include "version.h"

class ApplauncherProcess
:
    public QObject,
    public QnStoppable,
    public AbstractRequestProcessor,
    public TimerEventHandler
{
    Q_OBJECT

public:
    ApplauncherProcess(
        QSettings* const userSettings,
        InstallationManager* const installationManager,
        bool quitMode,
        const QString& mirrorListUrl );

    //!Implementation of \a ApplauncherProcess::pleaseStop()
    virtual void pleaseStop() override;

    //!Implementation of \a AbstractRequestProcessor::processRequest()
    virtual void processRequest(
        const std::shared_ptr<applauncher::api::BaseTask>& request,
        applauncher::api::Response** const response ) override;

    int run();

    QString devModeKey() const;
    void setDevModeKey(const QString &devModeKey);

private:
    struct KillProcessTask
    {
        qint64 processID;

        KillProcessTask()
        :
            processID( 0 )
        {
        }
    };

    bool m_terminated;
    InstallationManager* const m_installationManager;
    const bool m_quitMode;
    const QString m_mirrorListUrl;
    TaskServerNew m_taskServer;
    QSettings* const m_settings;
    int m_bindTriesCount;
    bool m_isLocalServerWasNotFound;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::atomic<unsigned int> m_prevInstallationID;
    std::map<unsigned int, std::shared_ptr<InstallationProcess>> m_activeInstallations;
    std::map<qint64, KillProcessTask> m_killProcessTasks;
    QString m_devModeKey;

    bool sendTaskToRunningLauncherInstance();
    bool getVersionToLaunch( QnSoftwareVersion* const versionToLaunch, QString* const appArgs );
    bool addTaskToThePipe( const QByteArray& serializedTask );
    bool startApplication(
        const std::shared_ptr<applauncher::api::StartApplicationTask>& task,
        applauncher::api::Response* const response );
    bool startInstallation(
        const std::shared_ptr<applauncher::api::StartInstallationTask>& task,
        applauncher::api::StartInstallationResponse* const response );
    bool installZip(
        const std::shared_ptr<applauncher::api::InstallZipTask>& request,
        applauncher::api::Response* const response );
    bool getInstallationStatus(
        const std::shared_ptr<applauncher::api::GetInstallationStatusRequest>& request,
        applauncher::api::InstallationStatusResponse* const response );
    bool isVersionInstalled(
        const std::shared_ptr<applauncher::api::IsVersionInstalledRequest>& request,
        applauncher::api::IsVersionInstalledResponse* const response );
    bool getInstalledVersions(
        const std::shared_ptr<applauncher::api::GetInstalledVersionsRequest>& request,
        applauncher::api::GetInstalledVersionsResponse* const response );
    bool cancelInstallation(
        const std::shared_ptr<applauncher::api::CancelInstallationRequest>& request,
        applauncher::api::CancelInstallationResponse* const response );
    bool addProcessKillTimer(
        const std::shared_ptr<applauncher::api::AddProcessKillTimerRequest>& request,
        applauncher::api::AddProcessKillTimerResponse* const response );
    bool blockingRestoreVersion( const QnSoftwareVersion& versionToLaunch );

    virtual void onTimer( const quint64& timerID ) override;

private slots:
    void onInstallationDone( InstallationProcess* installationProcess );
};

#endif  //APPLAUNCHER_PROCESS_H
