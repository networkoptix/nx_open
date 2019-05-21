#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <future>

#include <QObject>
#include <QSettings>

#include <nx/utils/software_version.h>
#include <nx/utils/thread/stoppable.h>
#include <nx/utils/timer_manager.h>

#include "abstract_request_processor.h"
#include "installation_manager.h"
#include "task_server_new.h"

#include <nx/vms/applauncher/api/applauncher_api.h>

#include <nx/utils/software_version.h>

namespace nx::applauncher {

class ApplauncherProcess:
    public QObject,
    public QnStoppable,
    public AbstractRequestProcessor,
    public nx::utils::TimerEventHandler
{
    Q_OBJECT

public:
    struct StartupParameters
    {
        enum class Mode
        {
            Default,
            Background,
            Quit
        };

        Mode mode = Mode::Default;
        int targetProtoVersion = 0;
        nx::utils::SoftwareVersion targetVersion;
        QStringList clientCommandLineParameters;
    };

    ApplauncherProcess(
        QSettings* const settings,
        InstallationManager* const installationManager,
        const StartupParameters& startupParameters);

    //!Implementation of \a ApplauncherProcess::pleaseStop()
    virtual void pleaseStop() override;

    //!Implementation of \a AbstractRequestProcessor::processRequest()
    virtual void processRequest(
        const std::shared_ptr<applauncher::api::BaseTask>& request,
        applauncher::api::Response** const response) override;

    int run();

private:
    void launchClient();

    /**
     * Find client version which should be launched right after applauncher start.
     * Logic is the following:
     *  * if version was provided and it exists, run it, otherwise:
     *  * if protocol version was provided, run the latest installed version with this protocol
     *  * otherwise run the latest installed version
     */
    nx::utils::SoftwareVersion getVersionToLaunch() const;

    bool startApplication(
        const std::shared_ptr<applauncher::api::StartApplicationTask>& task,
        applauncher::api::Response* const response);
    bool installZip(
        const std::shared_ptr<applauncher::api::InstallZipTask>& request,
        applauncher::api::Response* const response);
    bool installZipAsync(
        const std::shared_ptr<applauncher::api::InstallZipTaskAsync>& request,
        applauncher::api::Response* const response);
    bool checkInstallationProgress(
        const std::shared_ptr<applauncher::api::InstallZipCheckStatus>& request,
        applauncher::api::Response* const response);
    bool isVersionInstalled(
        const std::shared_ptr<applauncher::api::IsVersionInstalledRequest>& request,
        applauncher::api::IsVersionInstalledResponse* const response);
    bool getInstalledVersions(
        const std::shared_ptr<applauncher::api::GetInstalledVersionsRequest>& request,
        applauncher::api::GetInstalledVersionsResponse* const response);
    bool addProcessKillTimer(
        const std::shared_ptr<applauncher::api::AddProcessKillTimerRequest>& request,
        applauncher::api::AddProcessKillTimerResponse* const response);

    virtual void onTimer(const quint64& timerID) override;

private:
    struct KillProcessTask
    {
        qint64 processID = 0;
    };

    QSettings* const m_settings;
    InstallationManager* const m_installationManager;
    const StartupParameters m_startupParameters;
    TaskServerNew m_taskServer;

    bool m_terminated = false;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::map<qint64, KillProcessTask> m_killProcessTasks;

    /**
     * Wraps up information about active installation process.
     * Notice: We should not run another installation process until current one is finished.
     */
    struct InstallationProcess
    {
        /** Client version being installed. */
        nx::utils::SoftwareVersion version;
        /** Path to a zip file to be installed. */
        QString fileName;

        mutable std::mutex mutex;

        std::future<api::ResultType> result;

        void reset();
        bool isEmpty() const;
        bool equals(const nx::utils::SoftwareVersion& version, const QString& fileName) const;
    };

    InstallationProcess m_process;
};

} // namespace nx::applauncher
