// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <optional>

#include <QtCore/QObject>
#include <QtCore/QSettings>

#include <nx/reflect/string_conversion.h>
#include <nx/utils/software_version.h>
#include <nx/utils/thread/stoppable.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/applauncher/api/applauncher_api.h>

#include "installation_manager.h"
#include "task_server_new.h"

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
     * Task launching the version from the startup parameters if it is installed, otherwise the
     * latest installed version with the requested protocol, otherwise the latest one at all.
     */
    std::optional<applauncher::api::StartApplicationTask> clientLaunchTask() const;

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
    bool getInstalledVersionsEx(
        const applauncher::api::GetInstalledVersionsExRequest& /*request*/,
        applauncher::api::GetInstalledVersionsExResponse& response);
    bool addProcessKillTimer(
        const applauncher::api::AddProcessKillTimerRequest& request,
        applauncher::api::AddProcessKillTimerResponse& response);

    virtual void onTimer(const quint64& timerId) override;

    /**
     * Blocks until a quit command arrives. On macOS also requests the own quit when no client has
     * been running for a grace period to avoid "Running in Background" state.
     */
    void waitForTermination();

#if defined(Q_OS_MACOS)
    /** Whether a client is running or an installation is in progress. */
    bool isBusy() const;

    /** False if the request could not be delivered to the running instance. */
    bool delegateClientLaunch();

    /**
     * Asks the own task server to quit, its thread is blocked in accept() and only a request
     * wakes it up. Terminates the process if the request fails, since nothing else can stop it.
     */
    void requestQuit();
#endif

    template<class CallbackType>
    bool subscribe(applauncher::api::TaskType task,
        CallbackType&& callback)
    {
        return m_taskServer.subscribe(
            QByteArray::fromStdString(nx::reflect::toString(task)),
            std::move(callback));
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
    nx::utils::TimerManager m_timerManager;

#if defined(Q_OS_MACOS)
    /** Applauncher must not exit before this time point even if no client is running. */
    std::chrono::steady_clock::time_point m_keepAliveUntil;
    bool m_selfQuitRequested = false;
#endif
};

} // namespace nx::vms::applauncher
