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

namespace applauncher {

using namespace api;

ApplauncherProcess::ApplauncherProcess(
    QSettings* const settings,
    InstallationManager* const installationManager,
    const StartupParameters& startupParameters)
    :
    m_settings(settings),
    m_installationManager(installationManager),
    m_startupParameters(startupParameters),
    m_taskServer(this)
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

void ApplauncherProcess::processRequest(
    const std::shared_ptr<BaseTask>& request,
    Response** const response)
{
    switch (request->type)
    {
        case TaskType::quit:
            pleaseStop();
            break;

        case TaskType::startApplication:
            m_installationManager->updateInstalledVersionsInformation();
            *response = new Response();
            startApplication(
                std::static_pointer_cast<StartApplicationTask>(request),
                *response);
            break;

        case TaskType::installZip:
            *response = new Response();
            installZip(
                std::static_pointer_cast<InstallZipTask>(request),
                *response);
            break;

        case TaskType::isVersionInstalled:
            m_installationManager->updateInstalledVersionsInformation();
            *response = new IsVersionInstalledResponse();
            isVersionInstalled(
                std::static_pointer_cast<IsVersionInstalledRequest>(request),
                static_cast<IsVersionInstalledResponse*>(*response));
            break;

        case TaskType::getInstalledVersions:
            m_installationManager->updateInstalledVersionsInformation();
            *response = new GetInstalledVersionsResponse();
            getInstalledVersions(
                std::static_pointer_cast<GetInstalledVersionsRequest>(request),
                static_cast<GetInstalledVersionsResponse*>(*response));
            break;

        case TaskType::addProcessKillTimer:
            *response = new AddProcessKillTimerResponse();
            addProcessKillTimer(
                std::static_pointer_cast<AddProcessKillTimerRequest>(request),
                static_cast<AddProcessKillTimerResponse*>(*response));
            break;

        default:
            break;
    }
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
        const auto startAppTask = std::make_shared<StartApplicationTask>(
            versionToLaunch,
            m_startupParameters.clientCommandLineParameters);
        if (startApplication(startAppTask, &response))
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
    const std::shared_ptr<StartApplicationTask>& task,
    Response* const response)
{
    NX_VERBOSE(this, "Entered LaunchingApplication");

    auto installation = m_installationManager->installationForVersion(task->version);

    if (!installation)
    {
        installation = m_installationManager->installationForVersion(
            m_installationManager->nearestInstalledVersion(task->version));
    }

    if (!installation)
    {
        installation = m_installationManager->installationForVersion(
            m_installationManager->latestVersion());
    }

    if (installation.isNull())
    {
        NX_DEBUG(this, "Failed to find installed version %1", task->version);
        response->result = ResultType::versionNotInstalled;
        return false;
    }

    task->version = installation->version();

    //TODO/IMPL start process asynchronously ?

    const QString binPath = installation->executableFilePath();
    QStringList environment = QProcess::systemEnvironment();

    auto arguments = task->appArgs;

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

    NX_VERBOSE(this, "Launching version %1 (path %2)", task->version, binPath);

    const QFileInfo info(binPath);
    if (ProcessUtils::startProcessDetached(
        QnAppInfo::applicationPlatform() == "linux"
        ? "./" + info.fileName()
        : info.absoluteFilePath(),
        arguments,
        info.absolutePath(),
        environment))
    {
        NX_DEBUG(this, "Successfully launched version %1 (path %2)", task->version, binPath);
        m_settings->sync();
        response->result = ResultType::ok;
        return true;
    }

    NX_DEBUG(this, "Failed to launch version %1 (path %2)", task->version, binPath);
    response->result = ResultType::ioError;
    return false;
}

bool ApplauncherProcess::installZip(
    const std::shared_ptr<InstallZipTask>& request,
    Response* const response)
{
    response->result = m_installationManager->installZip(request->version, request->zipFileName);
    return response->result == ResultType::ok;
}

bool ApplauncherProcess::isVersionInstalled(
    const std::shared_ptr<IsVersionInstalledRequest>& request,
    IsVersionInstalledResponse* const response)
{
    response->installed = m_installationManager->isVersionInstalled(request->version);
    return true;
}

bool ApplauncherProcess::getInstalledVersions(
    const std::shared_ptr<GetInstalledVersionsRequest>& /*request*/,
    GetInstalledVersionsResponse* const response)
{
    response->versions = m_installationManager->installedVersions();
    return true;
}

bool ApplauncherProcess::addProcessKillTimer(
    const std::shared_ptr<AddProcessKillTimerRequest>& request,
    AddProcessKillTimerResponse* const response)
{
    KillProcessTask task;
    task.processID = request->processID;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_terminated)
        {
            response->result = ResultType::otherError;
            return true;
        }

        m_killProcessTasks[nx::utils::TimerManager::instance()->addTimer(
            this,
            std::chrono::milliseconds(request->timeoutMillis))] = task;
    }

    response->result = ResultType::ok;
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

} // namespace applauncher
