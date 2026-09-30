// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "applauncher_process.h"

#include <algorithm>
#include <cstdlib>
#include <thread>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/platform/process.h>
#include <nx/vms/applauncher/api/commands.h>

#include "process_utils.h"

namespace {

#if defined(Q_OS_MACOS)
constexpr auto kBusyPollInterval = std::chrono::seconds(3);

// Timeout for the client restart case.
constexpr auto kKeepAliveAfterClientExit = std::chrono::seconds(10);

// Extended timeout since Gatekeeper may delay the first start of a freshly installed client.
constexpr auto kKeepAliveAfterClientLaunch = std::chrono::seconds(60);

// An exiting instance closes its pipe before releasing its single instance lock.
constexpr auto kLockReleaseDelay = std::chrono::milliseconds(500);
#endif

struct RunParameters
{
    QString path;
    QString workingDirectory;
    QStringList arguments;
};

RunParameters getRunParameters(
    const QString& binaryPath,
    const nx::utils::SoftwareVersion& targetVersion,
    const QStringList& arguments)
{
    const QFileInfo info(binaryPath);

    if (nx::build_info::isLinux())
        return RunParameters{info.absoluteFilePath(), QDir::homePath(), arguments};

    if (nx::build_info::isWindows())
        return RunParameters{info.absoluteFilePath(), info.absolutePath(), arguments};

    // When we try to run MacOs client it runs global launcher of the application.
    // Global launcher looks for the newest applauncher and redirects request to it to
    // run the newest (or some specified) version. Up to 4.2 applauncher run client
    // by the direct executable call. The chain was user->initial client->applauncher->
    // final client. Because of that final client used settings from the initial one.
    // From 4.3 applauncher runs 4.3+ clients as a bundle which fixes settings usage.
    static const nx::utils::SoftwareVersion kMinimalBundleRunVersion(4,3);
    if (nx::build_info::isMacOsX() && targetVersion >= kMinimalBundleRunVersion)
    {
        auto bundleDir = info.absoluteDir();
        bundleDir.cd("../../");
        // Always run with "-n" (open a new instance) to simulate other OSes behavior.
        // All arguments after "--run" are passed to the client binary.
        const QStringList runArguments{"-n", "-a", bundleDir.absolutePath(), "--args", "--run"};
        return RunParameters{"open", bundleDir.absolutePath(), runArguments + arguments};
    }

    return RunParameters{info.absoluteFilePath(), info.absolutePath(), arguments};
}

} // namespace

namespace nx::vms::applauncher {

using namespace api;

ApplauncherProcess::ApplauncherProcess(
    QSettings* const settings,
    InstallationManager* const installationManager,
    const StartupParameters& startupParameters)
    :
    m_settings(settings),
    m_installationManager(installationManager),
    m_startupParameters(startupParameters),
    m_timerManager("ApplauncherProcess timer manager")
{
}

void ApplauncherProcess::pleaseStop()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_terminated = true;
        m_cond.notify_all();
    }

    std::for_each(
        m_killProcessTasks.begin(),
        m_killProcessTasks.end(),
        [this](const std::pair<qint64, KillProcessTask>& val)
        {
            m_timerManager.joinAndDeleteTimer(val.first);
        });
    m_killProcessTasks.clear();
}

void ApplauncherProcess::initChannels()
{
    m_taskServer.subscribeSimple(QByteArray::fromStdString(nx::reflect::toString(TaskType::quit)),
        [this]() -> bool
        {
#if defined(Q_OS_MACOS)
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                if (std::exchange(m_selfQuitRequested, false)
                    && std::chrono::steady_clock::now() < m_keepAliveUntil)
                {
                    return true; //< The own request was outrun by a client launch.
                }
            }
#endif
            NX_INFO(this, "processRequest() - received a command to stop. Exiting.");
            m_taskServer.pleaseStop();
            pleaseStop();
            return true;
        });

    subscribe(TaskType::pingApplauncher,
        [](const PingRequest& request, PingResponse& response)
        {
            response.pingId = request.pingId;
            auto now = std::chrono::system_clock::now().time_since_epoch();
            response.pingResponseStamp =
                std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            response.pingRequestStamp = request.pingStamp;
        });

    subscribe(TaskType::run,
        [this](const StartApplicationTask& request, Response& response)
        {
            m_installationManager->updateInstalledVersionsInformation();
            startApplication(request, response);
        });

    subscribe(TaskType::installZip,
        [this](const InstallZipTask& request, Response& response)
        {
            installZip(request, response);
        });

    subscribe(TaskType::startZipInstallation,
        [this](const InstallZipTaskAsync& request, Response& response)
        {
            installZipAsync(request, response);
        });

    subscribe(TaskType::checkZipProgress,
        [this](const InstallZipCheckStatus& request, InstallZipCheckStatusResponse& response)
        {
            checkInstallationProgress(request, response);
        });

    subscribe(TaskType::isVersionInstalled,
        [this](const IsVersionInstalledRequest& request, IsVersionInstalledResponse& response)
        {
            m_installationManager->updateInstalledVersionsInformation();
            isVersionInstalled(request, response);
        });

    subscribe(TaskType::getInstalledVersions,
        [this](const GetInstalledVersionsRequest& request, GetInstalledVersionsResponse& response)
        {
            m_installationManager->updateInstalledVersionsInformation();
            getInstalledVersions(request, response);
        });

    subscribe(TaskType::getInstalledVersionsEx,
        [this](const GetInstalledVersionsExRequest& request, GetInstalledVersionsExResponse& response)
        {
            m_installationManager->updateInstalledVersionsInformation();
            getInstalledVersionsEx(request, response);
        });

    subscribe(TaskType::addProcessKillTimer,
        [this](const AddProcessKillTimerRequest& request, AddProcessKillTimerResponse& response)
        {
            m_installationManager->updateInstalledVersionsInformation();
            addProcessKillTimer(request, response);
        });
}

void ApplauncherProcess::launchClient()
{
    const auto task = clientLaunchTask();
    if (!task)
        return;

    Response response;
    enum { kTriesCount = 2 };
    for (int i = 0; i < kTriesCount; ++i)
    {
        if (startApplication(*task, response))
            break;
    }
}

int ApplauncherProcess::run()
{
    const bool applauncherIsAlreadyStarted = !m_taskServer.listen(launcherPipeName());
    if (applauncherIsAlreadyStarted)
    {
        if (m_startupParameters.mode == StartupParameters::Mode::Quit)
        {
            NX_INFO(this, "run() - commanding another applauncher to exit");
            // Asking already-running instance to stop.
            return sendCommandToApplauncher(QuitTask()) == ResultType::ok;
        }

        if (m_startupParameters.mode != StartupParameters::Mode::Default)
            return 0;

        // Run the client (e.g. by shortcut click).
#if defined(Q_OS_MACOS)
        // The running instance launches the client so that it knows to stay alive.
        if (delegateClientLaunch())
            return 0;

        // The running instance is exiting, take its place.
        std::this_thread::sleep_for(kLockReleaseDelay);
        if (!m_taskServer.listen(launcherPipeName()))
            return 0;
#else
        launchClient();
        return 0;
#endif
    }

    // We are the only running applauncher instance.
    if (m_startupParameters.mode == StartupParameters::Mode::Quit)
        return 0;

    if (m_startupParameters.mode == StartupParameters::Mode::Default)
        launchClient();

    waitForTermination();

    // Waiting for all running tasks to stop.
    m_taskServer.pleaseStop();
    m_taskServer.wait();

    NX_INFO(this, "run() - exited gracefully");
    return 0;
}

void ApplauncherProcess::waitForTermination()
{
    std::unique_lock<std::mutex> lk(m_mutex);
#if defined(Q_OS_MACOS)
    m_keepAliveUntil =
        std::max(m_keepAliveUntil, std::chrono::steady_clock::now() + kKeepAliveAfterClientExit);
    while (!m_cond.wait_for(lk, kBusyPollInterval, [this]() { return m_terminated; }))
    {
        lk.unlock();
        const bool busy = isBusy();
        lk.lock();
        if (m_terminated)
            break;

        const auto now = std::chrono::steady_clock::now();
        if (busy)
            m_keepAliveUntil = std::max(m_keepAliveUntil, now + kKeepAliveAfterClientExit);
        if (busy || now < m_keepAliveUntil)
            continue;

        m_selfQuitRequested = true;
        lk.unlock();
        requestQuit();
        lk.lock();
    }
#else
    m_cond.wait(lk, [this]() { return m_terminated; });
#endif
}

#if defined(Q_OS_MACOS)

bool ApplauncherProcess::isBusy() const
{
    if (!m_process.getFile().isEmpty())
        return true; //< Installation is in progress.

    return ProcessUtils::isProcessRunning(nx::branding::desktopClientBinaryName()).value_or(true);
}

bool ApplauncherProcess::delegateClientLaunch()
{
    const auto task = clientLaunchTask();
    if (!task)
        return true;

    Response response;
    const auto result = sendCommandToApplauncher(*task, &response);
    if (result != ResultType::ok)
    {
        NX_WARNING(this, "Failed to send the client launch request: %1", result);
        return false;
    }

    if (response.result != ResultType::ok)
        NX_WARNING(this, "Running applauncher failed to launch the client: %1", response.result);
    return true;
}

void ApplauncherProcess::requestQuit()
{
    NX_INFO(this, "No client is running. Requesting exit.");
    const auto result = sendCommandToApplauncher(QuitTask());
    if (result == ResultType::ok)
        return;

    NX_ERROR(this, "Failed to request exit via the task server: %1. Terminating.", result);
    std::_Exit(1);
}

#endif // defined(Q_OS_MACOS)

std::optional<StartApplicationTask> ApplauncherProcess::clientLaunchTask() const
{
    const auto version =
        m_installationManager->latestVersion(m_startupParameters.targetProtoVersion);
    if (version.isNull())
        return std::nullopt;

    return StartApplicationTask(version, m_startupParameters.clientCommandLineParameters);
}

bool ApplauncherProcess::startApplication(
    const StartApplicationTask& task,
    Response& response)
{
    using namespace nx::utils;

    NX_VERBOSE(this, "Entered LaunchingApplication");

    auto installation = m_installationManager->installationForVersion(task.version);

    if (!installation)
    {
        installation = m_installationManager->installationForVersion(
            m_installationManager->nearestInstalledVersion(task.version));
    }

    if (!installation)
    {
        installation = m_installationManager->installationForVersion(
            m_installationManager->latestVersion());
    }

    if (installation.isNull())
    {
        NX_DEBUG(this, "Failed to find installed version %1", task.version);
        response.result = ResultType::versionNotInstalled;
        return false;
    }

    auto targetVersion = installation->version();

    //TODO/IMPL start process asynchronously ?

    const QString binPath = installation->executableFilePath();
    QStringList environment = QProcess::systemEnvironment();
    QStringList arguments = task.arguments;

    NX_VERBOSE(this, "Launching version %1 (path %2)", targetVersion, binPath);

    const auto runParams = getRunParameters(binPath, targetVersion, arguments);
    NX_DEBUG(this, "Launching with request: %1, args: %2", runParams.path,
        runParams.arguments.join(", "));

    if (ProcessUtils::startProcessDetached(
        runParams.path, runParams.arguments, runParams.workingDirectory, environment))
    {
        NX_DEBUG(this, "Successfully launched version %1 (path %2)", targetVersion, binPath);
        m_settings->sync();
        installation->updateLastExecutionTime();
#if defined(Q_OS_MACOS)
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_keepAliveUntil = std::max(
                m_keepAliveUntil, std::chrono::steady_clock::now() + kKeepAliveAfterClientLaunch);
        }
#endif
        response.result = ResultType::ok;
        return true;
    }

    NX_DEBUG(this, "Failed to launch version %1 (path %2)", targetVersion, binPath);
    response.result = ResultType::ioError;
    return false;
}

bool ApplauncherProcess::installZip(const InstallZipTask& request, Response& response)
{
    response.result = m_installationManager->installZip(request.version, request.zipFileName);
    return response.result == ResultType::ok;
}

bool ApplauncherProcess::installZipAsync(
    const InstallZipTaskAsync& request,
    Response& response)
{
    bool canStartInstallation = false;
    if (!m_process.isEmpty())
    {
        // It is ok if we are already installing the same file and version.
        if (m_process.equals(request.version, request.zipFileName))
        {
            NX_INFO(this, "installZipAsync() - already installing version %1 from file %2",
                request.version, request.zipFileName);
            response.result = ResultType::ok;
        }
        else if (
            m_process.result.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
        {
            auto result = m_process.result.get();
            NX_VERBOSE(this, "installZipAsync() - previous installation just ended, result=%1",
                result);
            canStartInstallation = true;
        }
        else
        {
            NX_ERROR(this, "installZipAsync() - can not start installing version %1 from %2: "
                    "already installing from %3", request.version, request.zipFileName,
                    m_process.getFile());
            response.result = ResultType::busy;
        }
    }
    else
    {
        canStartInstallation = true;
    }

    if (canStartInstallation)
    {
        NX_VERBOSE(this, "Starting async installation for version %1 from file %2",
            request.version, request.zipFileName);
        m_process.result = std::async(std::launch::async,
            [this, request]() -> ResultType
            {
                {
                    std::scoped_lock<std::mutex> lock(m_process.mutex);
                    m_process.version = request.version;
                    m_process.fileName = request.zipFileName;
                }
                auto result = m_installationManager->installZip(request.version, request.zipFileName);
                m_process.reset();
                return result;
            });
        response.result = ResultType::ok;
        NX_INFO(this, "Started async installation for version %1 from file %2",
            request.version, request.zipFileName);
    }

    return response.result == ResultType::ok;
}

bool ApplauncherProcess::checkInstallationProgress(
    const nx::vms::applauncher::api::InstallZipCheckStatus& request,
    applauncher::api::InstallZipCheckStatusResponse& response)
{
    if (!m_process.isEmpty())
    {
        if (m_process.result.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
        {
            response.result = m_process.result.get();
            NX_DEBUG(this,
                "checkInstallationProgress() - complete, result=%1",
                response.result);
        }
        else
        {
            QString fileName = m_process.getFile();
            response.result = ResultType::unpackingZip;
            NX_DEBUG(this,
                "checkInstallationProgress() - still installing %1", fileName);
        }

        response.total = m_installationManager->getBytesTotal();
        response.extracted = m_installationManager->getBytesExtracted();
    }
    else
    {
        bool installed = m_installationManager->isVersionInstalled(request.version);
        response.result = installed ? ResultType::ok : ResultType::versionNotInstalled;
        NX_DEBUG(this,
            "checkInstallationProgress() - there is no active installation");
    }

    return response.result == ResultType::ok;
}

bool ApplauncherProcess::isVersionInstalled(
    const IsVersionInstalledRequest& request,
    IsVersionInstalledResponse& response)
{
    response.installed = m_installationManager->isVersionInstalled(request.version);
    return true;
}

bool ApplauncherProcess::getInstalledVersions(
    const GetInstalledVersionsRequest& /*request*/,
    GetInstalledVersionsResponse& response)
{
    response.versions = m_installationManager->installedVersions();
    return true;
}

bool ApplauncherProcess::getInstalledVersionsEx(
    const GetInstalledVersionsExRequest& /*request*/,
    GetInstalledVersionsExResponse& response)
{
    for (const auto& installation : m_installationManager->installations())
    {
        if (installation->exists())
        {
            response.versions.push_back(ClientVersionInfo{
                .version = installation->version(),
                .protocolVersion = installation->protocolVersion()});
        }
    }

    return true;
}

bool ApplauncherProcess::addProcessKillTimer(
    const AddProcessKillTimerRequest& request,
    AddProcessKillTimerResponse& response)
{
    KillProcessTask task;
    task.processID = request.processId;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_terminated)
        {
            response.result = ResultType::otherError;
            return true;
        }

        m_killProcessTasks[m_timerManager.addTimer(
            this,
            std::chrono::milliseconds(request.timeoutMillis))] = task;
    }

    response.result = ResultType::ok;
    return true;
}

void ApplauncherProcess::onTimer(const quint64& timerId)
{
    KillProcessTask task;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_terminated)
            return;

        auto it = m_killProcessTasks.find(timerId);
        if (it == m_killProcessTasks.end())
            return;
        task = it->second;
        m_killProcessTasks.erase(it);
    }

    // Stopping process if needed.
    const auto code = nx::utils::killProcessByPid(task.processID);
    static_cast<void>(code);
}

QString ApplauncherProcess::InstallationProcess::getFile() const
{
    std::scoped_lock<std::mutex> lock(mutex);
    return fileName;
}

void ApplauncherProcess::InstallationProcess::reset()
{
    std::scoped_lock<std::mutex> lock(mutex);
    version = {};
    fileName = "";
}

bool ApplauncherProcess::InstallationProcess::isEmpty() const
{
    return !result.valid();
}

bool ApplauncherProcess::InstallationProcess::equals(
    const nx::utils::SoftwareVersion& version, const QString& fileName) const
{
    std::scoped_lock<std::mutex> lock(mutex);
    return this->version == version && this->fileName == fileName;
}

} // namespace nx::vms::applauncher
