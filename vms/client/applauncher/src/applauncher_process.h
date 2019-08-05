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

#include "installation_manager.h"
#include "task_server_new.h"

#include <nx/vms/applauncher/api/applauncher_api.h>

#include <nx/utils/software_version.h>

namespace nx::vms::applauncher {

class ApplauncherProcess:
    public QObject,
    public QnStoppable,
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

    int run();
    void initChannels();

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
        const applauncher::api::StartApplicationTask& task,
        applauncher::api::Response& response);
    bool installZip(
        const applauncher::api::InstallZipTask& request,
        applauncher::api::Response& response);
    bool installZipAsync(
        const applauncher::api::InstallZipTaskAsync& request,
        applauncher::api::Response& response);
    bool checkInstallationProgress(
        const applauncher::api::InstallZipCheckStatus& request,
        applauncher::api::InstallZipCheckStatusResponse& response);
    bool isVersionInstalled(
        const applauncher::api::IsVersionInstalledRequest& request,
        applauncher::api::IsVersionInstalledResponse& response);
    bool getInstalledVersions(
        const applauncher::api::GetInstalledVersionsRequest& request,
        applauncher::api::GetInstalledVersionsResponse& response);
    bool addProcessKillTimer(
        const applauncher::api::AddProcessKillTimerRequest& request,
        applauncher::api::AddProcessKillTimerResponse& response);

    virtual void onTimer(const quint64& timerID) override;

    template<class RequestType, class ResponseType>
    bool subscribe(applauncher::api::TaskType task,
        std::function<void (const RequestType&, ResponseType&)>&& callback)
    {
        return m_taskServer.subscribe(serializeTaskType(task), std::move(callback));
    }

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

        QString getFile() const;
        void reset();
        bool isEmpty() const;
        bool equals(const nx::utils::SoftwareVersion& version, const QString& fileName) const;
    };

    InstallationProcess m_process;
};

} // namespace nx::vms::applauncher
