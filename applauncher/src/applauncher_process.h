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

#include "abstract_request_processor.h"
#include "installation_manager.h"
#include "installation_process.h"
#include "task_server_new.h"


class ApplauncherProcess
:
    public QObject,
    public QnStoppable,
    public AbstractRequestProcessor
{
    Q_OBJECT

public:
    ApplauncherProcess(
        QSettings* const settings,
        InstallationManager* const installationManager,
        bool quitMode );

    //!Implementation of \a ApplauncherProcess::pleaseStop()
    virtual void pleaseStop() override;

    //!Implementation of \a AbstractRequestProcessor::processRequest()
    virtual void processRequest(
        const std::shared_ptr<applauncher::api::BaseTask>& request,
        applauncher::api::Response** const response ) override;

    int run();

private:
    bool m_terminated;
    InstallationManager* const m_installationManager;
    const bool m_quitMode;
    TaskServerNew m_taskServer;
    QSettings* const m_settings;
    int m_bindTriesCount;
    bool m_isLocalServerWasNotFound;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::atomic<unsigned int> m_prevInstallationID;
    std::map<unsigned int, std::shared_ptr<InstallationProcess>> m_activeInstallations;

    bool sendTaskToRunningLauncherInstance();
    bool getVersionToLaunch( QString* const versionToLaunch, QString* const appArgs );
    bool addTaskToThePipe( const QByteArray& serializedTask );
    bool startApplication(
        const std::shared_ptr<applauncher::api::StartApplicationTask>& task,
        applauncher::api::Response* const response );
    bool startInstallation(
        const std::shared_ptr<applauncher::api::StartInstallationTask>& task,
        applauncher::api::StartInstallationResponse* const response );
    bool getInstallationStatus(
        const std::shared_ptr<applauncher::api::GetInstallationStatusRequest>& request,
        applauncher::api::InstallationStatusResponse* const response );
    bool isVersionInstalled(
        const std::shared_ptr<applauncher::api::IsVersionInstalledRequest>& request,
        applauncher::api::IsVersionInstalledResponse* const response );
    bool cancelInstallation(
        const std::shared_ptr<applauncher::api::CancelInstallationRequest>& request,
        applauncher::api::CancelInstallationResponse* const response );

private slots:
    void onInstallationSucceeded();
};

#endif  //APPLAUNCHER_PROCESS_H
