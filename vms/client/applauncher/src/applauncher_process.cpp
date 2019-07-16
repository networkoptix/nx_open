#include "applauncher_process.h"

#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>
#include <QtCore/QCoreApplication>

#include <nx/utils/log/log.h>
#include <utils/common/process.h>
#include <utils/common/app_info.h>

#include "process_utils.h"

#include <nx/vms/applauncher/api/commands.h>

namespace {

/*! Since 3.0 client-bin uses relative rpath to specify its libs location.
    Thus we don't have to put it into LD_LIBRARY_PATH. */
const nx::utils::SoftwareVersion kRpathIncludedVersion(3, 0);

/*! Since 3.0 client uses correct window class name depending on its customization.
    Previous versions use executable name as WM class (client-bin) which won't work properly
    for startup notification protocol (which is used in many Linux distros launchers).
    For these versions we pass -name <wmclass> parameter which sets the correct WM class
    to windows. */
const nx::utils::SoftwareVersion kWindowClassFixedVersion(3, 0);

/**
 * Since 4.0 client uses GStreamer 1.0 and is compatible to Ubuntu 18.04. Previous clients require
 * GStreamer 0.10 which is not available in Ubuntu 18.04. Previous Ubuntu versions had GStreamer
 * 0.10 in repos but it didn't have to be installed. For such systems we have to provide GStreamer
 * libraries manually. Applauncher stores them in a separate directory and brings them to client
 * instances via LD_LIBRARY_PATH.
 */
const nx::utils::SoftwareVersion kNewGstreamerUsedVersion(4, 0);

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
    m_startupParameters(startupParameters)
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
        [](const std::pair<qint64, KillProcessTask>& val)
        {
            nx::utils::TimerManager::instance()->joinAndDeleteTimer(val.first);
        });
    m_killProcessTasks.clear();
}

void ApplauncherProcess::initChannels()
{
    m_taskServer.subscribe(serializeTaskType(TaskType::quit),
        [this](const QByteArray& /*data*/) -> bool
        {
            NX_INFO(this, "processRequest() - received a command to stop. Exiting.");
            m_taskServer.pleaseStop();
            pleaseStop();
            return true;
        });

    subscribe<PingRequest, PingResponse>(TaskType::pingApplauncher,
        [](const PingRequest& request, PingResponse& response)
        {
            response.pingId = request.pingId;
            response.pingRequestStamp = request.pingStamp;
        });

    subscribe<StartApplicationTask, Response>(TaskType::startApplication,
        [this](const StartApplicationTask& request, Response& response)
        {
            m_installationManager->updateInstalledVersionsInformation();
            startApplication(request, response);
        });

    subscribe<StartApplicationTask, Response>(TaskType::startApplication,
        [this](const StartApplicationTask& request, Response& response)
        {
            m_installationManager->updateInstalledVersionsInformation();
            startApplication(request, response);
        });

    subscribe<InstallZipTask, Response>(
        TaskType::installZip,
        [this](const InstallZipTask& request, Response& response)
        {
            installZip(request, response);
        });

    subscribe<InstallZipTaskAsync, Response>(
        TaskType::startZipInstallation,
        [this](const InstallZipTaskAsync& request, Response& response)
        {
            installZipAsync(request, response);
        });

    subscribe<InstallZipCheckStatus, InstallZipCheckStatusResponse>(
        TaskType::checkZipProgress,
        [this](const InstallZipCheckStatus& request, InstallZipCheckStatusResponse& response)
        {
            checkInstallationProgress(request, response);
        });

    subscribe<IsVersionInstalledRequest, IsVersionInstalledResponse>(
        TaskType::isVersionInstalled,
        [this](const IsVersionInstalledRequest& request, IsVersionInstalledResponse& response)
        {
            m_installationManager->updateInstalledVersionsInformation();
            isVersionInstalled(request, response);
        });

    subscribe<GetInstalledVersionsRequest, GetInstalledVersionsResponse>(
        TaskType::getInstalledVersions,
        [this](const GetInstalledVersionsRequest& request, GetInstalledVersionsResponse& response)
        {
            m_installationManager->updateInstalledVersionsInformation();
            getInstalledVersions(request, response);
        });

    subscribe<AddProcessKillTimerRequest, AddProcessKillTimerResponse>(
        TaskType::addProcessKillTimer,
        [this](const AddProcessKillTimerRequest& request, AddProcessKillTimerResponse& response)
        {
            m_installationManager->updateInstalledVersionsInformation();
            addProcessKillTimer(request, response);
        });
}

void ApplauncherProcess::launchClient()
{
    nx::utils::SoftwareVersion versionToLaunch = getVersionToLaunch();
    if (versionToLaunch.isNull())
        return;

    Response response;
    enum { kTriesCount = 2 };
    for (int i = 0; i < kTriesCount; ++i)
    {
        StartApplicationTask startAppTask(versionToLaunch,
            m_startupParameters.clientCommandLineParameters);
        if (startApplication(startAppTask, response))
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

        // Run the client (e.g. by shortcut click).
        if (m_startupParameters.mode == StartupParameters::Mode::Default)
            launchClient();

        return 0;
    }

    // We are the only running applauncher instance.
    if (m_startupParameters.mode == StartupParameters::Mode::Quit)
        return 0;

    if (m_startupParameters.mode == StartupParameters::Mode::Default)
        launchClient();

    std::unique_lock<std::mutex> lk(m_mutex);
    m_cond.wait(lk, [this]() { return m_terminated; });

    // Waiting for all running tasks to stop.
    m_taskServer.pleaseStop();
    m_taskServer.wait();

    NX_ALWAYS(this, "run() - exited gracefully");

    return 0;
}

nx::utils::SoftwareVersion ApplauncherProcess::getVersionToLaunch() const
{
    if (!m_startupParameters.targetVersion.isNull())
    {
        if (m_installationManager->isVersionInstalled(m_startupParameters.targetVersion, true))
            return m_startupParameters.targetVersion;
    }

    return m_installationManager->latestVersion(m_startupParameters.targetProtoVersion);
}

bool ApplauncherProcess::startApplication(
    const StartApplicationTask& task,
    Response& response)
{
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

    auto arguments = task.arguments;

    if (QnAppInfo::applicationPlatform() == "linux")
    {
        const QString kLdLibraryPathVariable = "LD_LIBRARY_PATH";
        QStringList ldLibraryPaths;

        if (installation->version() < kRpathIncludedVersion)
        {
            const QString clientLibPath = installation->libraryPath();

            if (!clientLibPath.isEmpty() && QFile::exists(clientLibPath))
                ldLibraryPaths.append(clientLibPath);
        }

        if (installation->version() < kNewGstreamerUsedVersion)
        {
            const QString gstreamerLibsLibsPath =
                QDir(qApp->applicationDirPath()).absoluteFilePath("../lib/gstreamer-0.10");

            ldLibraryPaths.append(gstreamerLibsLibsPath);
        }

        if (installation->version() < kWindowClassFixedVersion)
        {
            arguments.append("-name");
            arguments.append(QnAppInfo::productNameShort());
        }

        if (!ldLibraryPaths.isEmpty())
        {
            const QString ldLibraryPath = ldLibraryPaths.join(':');

            QRegularExpression varRegExp(QString("%1=(.+)").arg(kLdLibraryPathVariable));

            auto it = environment.begin();
            for (; it != environment.end(); ++it)
            {
                const QRegularExpressionMatch match = varRegExp.match(*it);
                if (match.isValid())
                {
                    *it = QString("%1=%2:%3").arg(
                        kLdLibraryPathVariable,
                        ldLibraryPath,
                        match.captured(1));
                    break;
                }
            }

            if (it == environment.end())
                environment.append(QString("%1=%2").arg(kLdLibraryPathVariable, ldLibraryPath));
        }
    }

    NX_VERBOSE(this, "Launching version %1 (path %2)", targetVersion, binPath);

    const QFileInfo info(binPath);
    if (ProcessUtils::startProcessDetached(
        QnAppInfo::applicationPlatform() == "linux"
        ? "./" + info.fileName()
        : info.absoluteFilePath(),
        arguments,
        info.absolutePath(),
        environment))
    {
        NX_DEBUG(this, "Successfully launched version %1 (path %2)", targetVersion, binPath);
        m_settings->sync();
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
    const nx::vms::applauncher::api::InstallZipCheckStatus& /*request*/,
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
        response.result = ResultType::ok;
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

        m_killProcessTasks[nx::utils::TimerManager::instance()->addTimer(
            this,
            std::chrono::milliseconds(request.timeoutMillis))] = task;
    }

    response.result = ResultType::ok;
    return true;
}

void ApplauncherProcess::onTimer(const quint64& timerID)
{
    KillProcessTask task;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_terminated)
            return;

        auto it = m_killProcessTasks.find(timerID);
        if (it == m_killProcessTasks.end())
            return;
        task = it->second;
        m_killProcessTasks.erase(it);
    }

    // Stopping process if needed.
    const auto code = nx::killProcessByPid(task.processID);
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
