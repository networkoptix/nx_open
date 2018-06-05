/**********************************************************
* 20 sep 2013
* a.kolesnikov
***********************************************************/

#include "applauncher_process.h"

#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtNetwork/QLocalSocket>

#include <api/ipc_pipe_names.h>
#include <nx/utils/log/log.h>
#include <utils/common/process.h>
#include <utils/common/app_info.h>

#include "process_utils.h"

namespace {

/*! Since 3.0 client-bin uses relative rpath to specify its libs location.
    Thus we don't have to put it into LD_LIBRARY_PATH. */
const QnSoftwareVersion kRpathIncludedVersion(3, 0);
/*! Since 3.0 client uses correct window class name depending on its customization.
    Previous versions use executable name as WM class (client-bin) which won't work properly
    for startup notification protocol (which is used in many Linux distros launchers).
    For these versions we pass -name <wmclass> parameter which sets the correct WM class
    to windows. */
const QnSoftwareVersion kWindowClassFixedVersion(3, 0);

} // namespace


ApplauncherProcess::ApplauncherProcess(
    QSettings* const settings,
    InstallationManager* const installationManager,
    Mode mode,
    const QStringList& applicationParameters,
    const QString& mirrorListUrl)
    :
    m_terminated(false),
    m_installationManager(installationManager),
    m_mode(mode),
    m_mirrorListUrl(mirrorListUrl),
    m_taskServer(this),
    m_settings(settings),
    m_bindTriesCount(0),
    m_isLocalServerWasNotFound(false),
    m_applicationParameters(applicationParameters)
{}

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
        [](const std::pair<qint64, KillProcessTask>& val) { nx::utils::TimerManager::instance()->joinAndDeleteTimer(val.first); });
    m_killProcessTasks.clear();
}

void ApplauncherProcess::processRequest(
    const std::shared_ptr<applauncher::api::BaseTask>& request,
    applauncher::api::Response** const response)
{
    switch (request->type)
    {
        case applauncher::api::TaskType::quit:
            pleaseStop();
            break;

        case applauncher::api::TaskType::startApplication:
            m_installationManager->updateInstalledVersionsInformation();
            *response = new applauncher::api::Response();
            startApplication(
                std::static_pointer_cast<applauncher::api::StartApplicationTask>(request),
                *response);
            break;

        case applauncher::api::TaskType::install:
            *response = new applauncher::api::StartInstallationResponse();
            startInstallation(
                std::static_pointer_cast<applauncher::api::StartInstallationTask>(request),
                static_cast<applauncher::api::StartInstallationResponse*>(*response));
            break;

        case applauncher::api::TaskType::installZip:
            *response = new applauncher::api::Response();
            installZip(
                std::static_pointer_cast<applauncher::api::InstallZipTask>(request),
                static_cast<applauncher::api::Response*>(*response));
            break;

        case applauncher::api::TaskType::getInstallationStatus:
            *response = new applauncher::api::InstallationStatusResponse();
            getInstallationStatus(
                std::static_pointer_cast<applauncher::api::GetInstallationStatusRequest>(request),
                static_cast<applauncher::api::InstallationStatusResponse*>(*response));
            break;

        case applauncher::api::TaskType::isVersionInstalled:
            m_installationManager->updateInstalledVersionsInformation();
            *response = new applauncher::api::IsVersionInstalledResponse();
            isVersionInstalled(
                std::static_pointer_cast<applauncher::api::IsVersionInstalledRequest>(request),
                static_cast<applauncher::api::IsVersionInstalledResponse*>(*response));
            break;

        case applauncher::api::TaskType::getInstalledVersions:
            m_installationManager->updateInstalledVersionsInformation();
            *response = new applauncher::api::GetInstalledVersionsResponse();
            getInstalledVersions(
                std::static_pointer_cast<applauncher::api::GetInstalledVersionsRequest>(request),
                static_cast<applauncher::api::GetInstalledVersionsResponse*>(*response));
            break;

        case applauncher::api::TaskType::cancelInstallation:
            *response = new applauncher::api::CancelInstallationResponse();
            cancelInstallation(
                std::static_pointer_cast<applauncher::api::CancelInstallationRequest>(request),
                static_cast<applauncher::api::CancelInstallationResponse*>(*response));
            break;

        case applauncher::api::TaskType::addProcessKillTimer:
            *response = new applauncher::api::AddProcessKillTimerResponse();
            addProcessKillTimer(
                std::static_pointer_cast<applauncher::api::AddProcessKillTimerRequest>(request),
                static_cast<applauncher::api::AddProcessKillTimerResponse*>(*response));
            break;

        default:
            break;
    }
}

void ApplauncherProcess::launchNewestClient()
{
    QnSoftwareVersion versionToLaunch;
    if (!getVersionToLaunch(&versionToLaunch))
        return;

    applauncher::api::Response response;
    enum { kTriesCount = 2 };
    for (int i = 0; i < kTriesCount; ++i)
    {
        const auto startAppTask = std::make_shared<applauncher::api::StartApplicationTask>(
            versionToLaunch, m_applicationParameters);
        if (startApplication(startAppTask, &response))
            break;

        //failed to start, trying to restore version
        if (!blockingRestoreVersion(versionToLaunch))
            versionToLaunch = m_installationManager->latestVersion();
    }
}

int ApplauncherProcess::run()
{
    const bool clientIsRunAlready = !m_taskServer.listen(launcherPipeName());
    if (clientIsRunAlready)
    {
        if (m_mode == Mode::Quit)
        {
            //asking already-running instance to stop
            return addTaskToThePipe(applauncher::api::QuitTask().serialize());
        }

        if (m_mode == Mode::Default) //< another instance already running?
            launchNewestClient();

        return 0;
    }

    //we are the only running instance
    if (m_mode == Mode::Quit)
        return 0;

    if (m_mode == Mode::Default)
        launchNewestClient();

    std::unique_lock<std::mutex> lk(m_mutex);
    m_cond.wait(lk, [this]() { return m_terminated; });

    //waiting for all running tasks to stop
    m_taskServer.pleaseStop();
    m_taskServer.wait();

    //interrupting running installation tasks. With taskserver down no one can modify m_activeInstallations
    for (auto installationIter : m_activeInstallations)
        installationIter.second->pleaseStop();

    for (auto installationIter : m_activeInstallations)
        installationIter.second->join();

    m_activeInstallations.clear();

    return 0;
}

QString ApplauncherProcess::devModeKey() const
{
    return m_devModeKey;
}

void ApplauncherProcess::setDevModeKey(const QString &devModeKey)
{
    m_devModeKey = devModeKey;
}

bool ApplauncherProcess::sendTaskToRunningLauncherInstance()
{
    NX_LOG(QString::fromLatin1("Entered AddingTaskToNamedPipe"), cl_logDEBUG1);

    QByteArray serializedTask;
    if (m_mode != Mode::Quit)
    {
        QnSoftwareVersion versionToLaunch;
        if (!getVersionToLaunch(&versionToLaunch))
        {
            NX_LOG(QString::fromLatin1("Failed to find what to launch. Will not post any task to the named pipe"), cl_logDEBUG1);
            return false;
        }

        applauncher::api::StartApplicationTask startTask(versionToLaunch);
        startTask.autoRestore = true;
        serializedTask = startTask.serialize();
    }
    else
    {
        serializedTask = applauncher::api::QuitTask().serialize();
    }

    return addTaskToThePipe(serializedTask);
}

static const QLatin1String MOST_RECENT_VERSION_PARAM_NAME("mostRecentVersion");

bool ApplauncherProcess::getVersionToLaunch(QnSoftwareVersion* const versionToLaunch)
{
    if (m_settings->contains(MOST_RECENT_VERSION_PARAM_NAME))
    {
        const QnSoftwareVersion& previousMostRecentVersion =
            QnSoftwareVersion(m_settings->value(MOST_RECENT_VERSION_PARAM_NAME).toString());
        if (previousMostRecentVersion < m_installationManager->latestVersion())
        {
            //newer version have been installed since previous client start,
            //ignoring previous launched version and running the new one
            *versionToLaunch = m_installationManager->latestVersion();
            return true;
        }
    }

    if (m_installationManager->count() > 0)
    {
        *versionToLaunch = m_installationManager->latestVersion();
        //leaving default cmd params
    }
    else
    {
        NX_LOG(QString::fromLatin1("Failed to generate launch task. No client installed"), cl_logDEBUG1);
        return false;
    }

    return true;
}

static const int MAX_MSG_LEN = 1024;

bool ApplauncherProcess::addTaskToThePipe(const QByteArray& serializedTask)
{
    //posting to the pipe
#ifdef _WIN32
    NamedPipeSocket sock;
    SystemError::ErrorCode result = sock.connectToServerSync(launcherPipeName());
    if (result != SystemError::noError)
    {
        m_isLocalServerWasNotFound = result == SystemError::fileNotFound;
        NX_LOG(lm("Failed to connect to local server %1. %2")
            .args(launcherPipeName(), SystemError::toString(result)), cl_logDEBUG1);
        return false;
    }

    unsigned int bytesWritten = 0;
    result = sock.write(serializedTask.constData(), serializedTask.size(), &bytesWritten);
    if ((result != SystemError::noError) || (bytesWritten != serializedTask.size()))
    {
        m_isLocalServerWasNotFound = result == SystemError::fileNotFound;
        NX_LOG(lm("Failed to send launch task to local server %1. %2")
            .args(launcherPipeName(), SystemError::toString(result)), cl_logDEBUG1);
        return false;
    }

    char buf[MAX_MSG_LEN];
    unsigned int bytesRead = 0;
    sock.read(buf, sizeof(buf), &bytesRead);  //ignoring return code

    return true;
#else
    QLocalSocket sock;
    sock.connectToServer(launcherPipeName());
    if (!sock.waitForConnected(-1))
    {
        m_isLocalServerWasNotFound = sock.error() == QLocalSocket::ServerNotFoundError ||
            sock.error() == QLocalSocket::ConnectionRefusedError ||
            sock.error() == QLocalSocket::PeerClosedError;
        NX_LOG(QString::fromLatin1("Failed to connect to local server %1. %2").arg(launcherPipeName()).arg(sock.errorString()), cl_logDEBUG1);
        return false;
    }

    if (sock.write(serializedTask.data(), serializedTask.size()) != serializedTask.size())
    {
        m_isLocalServerWasNotFound = sock.error() == QLocalSocket::ServerNotFoundError ||
            sock.error() == QLocalSocket::ConnectionRefusedError ||
            sock.error() == QLocalSocket::PeerClosedError;
        NX_LOG(QString::fromLatin1("Failed to send launch task to local server %1. %2").arg(launcherPipeName()).arg(sock.errorString()), cl_logDEBUG1);
        return false;
    }

    sock.waitForReadyRead(-1);
    sock.readAll();

    return true;
#endif
}

static const QLatin1String NON_RECENT_VERSION_ARGS_PARAM_NAME("nonRecentVersionArgs");
static const QLatin1String NON_RECENT_VERSION_ARGS_DEFAULT_VALUE("--updates-enabled=false");

bool ApplauncherProcess::startApplication(
    const std::shared_ptr<applauncher::api::StartApplicationTask>& task,
    applauncher::api::Response* const response)
{
    NX_LOG(QString::fromLatin1("Entered LaunchingApplication"), cl_logDEBUG1);

#ifdef AK_DEBUG
    std::const_pointer_cast<applauncher::api::StartApplicationTask>(task)->version = "debug";
#endif

    QnClientInstallationPtr installation = m_installationManager->installationForVersion(task->version);
    if (installation.isNull())
        installation = m_installationManager->installationForVersion(m_installationManager->latestVersion());

    if (installation.isNull())
    {
        NX_LOG(QString::fromLatin1("Failed to find installed version %1 path").arg(task->version.toString()), cl_logDEBUG1);
        response->result = applauncher::api::ResultType::versionNotInstalled;
        return false;
    }

    task->version = installation->version();

    if (task->version != m_installationManager->latestVersion())
    {
        task->appArgs += QString::fromLatin1(" ") + m_settings->value(NON_RECENT_VERSION_ARGS_PARAM_NAME, NON_RECENT_VERSION_ARGS_DEFAULT_VALUE).toString();

        if (installation->isNeedsVerification() && !installation->verify())
        {
            NX_LOG(QString::fromLatin1("Verification failed for version %1 (path %2)").arg(installation->version().toString()).arg(installation->rootPath()), cl_logDEBUG1);
            response->result = applauncher::api::ResultType::ioError;

            if (task->autoRestore)
            {
                applauncher::api::StartInstallationResponse startInstallationResponse;
                startInstallation(
                    std::make_shared<applauncher::api::StartInstallationTask>(task->version, true),
                    &startInstallationResponse);
            }

            return false;
        }
    }

    //TODO/IMPL start process asynchronously ?

    const QString binPath = installation->executableFilePath();
    QStringList environment = QProcess::systemEnvironment();


    auto arguments = task->appArgs;
    if (!m_devModeKey.isEmpty())
        arguments.append(QString::fromLatin1("--dev-mode-key=%1").arg(devModeKey()));

    if (QnAppInfo::applicationPlatform() == "linux")
    {
        if (installation->version() < kRpathIncludedVersion)
        {
            QString ldLibraryPath = installation->libraryPath();
            if (!ldLibraryPath.isEmpty() && QFile::exists(ldLibraryPath))
            {
                const QString kLdLibraryPathVariable = "LD_LIBRARY_PATH";

                QRegExp varRegExp(QString("%1=(.+)").arg(kLdLibraryPathVariable));

                auto it = environment.begin();
                for (; it != environment.end(); ++it)
                {
                    if (varRegExp.exactMatch(*it))
                    {
                        *it = QString("%1=%2:%3").arg(
                            kLdLibraryPathVariable, ldLibraryPath, varRegExp.cap(1));
                        break;
                    }
                }
                if (it == environment.end())
                {
                    environment.append(
                        QString("%1=%2").arg(kLdLibraryPathVariable, ldLibraryPath));
                }
            }
        }

        if (installation->version() < kWindowClassFixedVersion)
        {
            arguments.append("-name");
            arguments.append(QnAppInfo::productNameShort());
        }
    }

    NX_LOG(QString::fromLatin1("Launching version %1 (path %2)").arg(task->version.toString()).arg(binPath), cl_logDEBUG2);

    const QFileInfo info(binPath);
    if (ProcessUtils::startProcessDetached(
        QnAppInfo::applicationPlatform() == "linux"
            ? "./" + info.fileName() : info.absoluteFilePath(),
        arguments,
        info.absolutePath(),
        environment))
    {
        NX_LOG(QString::fromLatin1("Successfully launched version %1 (path %2)").arg(task->version.toString()).arg(binPath), cl_logDEBUG1);
        m_settings->setValue(MOST_RECENT_VERSION_PARAM_NAME, m_installationManager->latestVersion().toString());
        m_settings->sync();
        response->result = applauncher::api::ResultType::ok;
        return true;
    }
    else
    {
        //TODO/IMPL should mark version as not installed or corrupted?
        NX_LOG(QString::fromLatin1("Failed to launch version %1 (path %2)").arg(task->version.toString()).arg(binPath), cl_logDEBUG1);
        response->result = applauncher::api::ResultType::ioError;

        if (task->autoRestore)
        {
            applauncher::api::StartInstallationResponse startInstallationResponse;
            startInstallation(
                std::make_shared<applauncher::api::StartInstallationTask>(task->version, true),
                &startInstallationResponse);
        }

        return false;
    }
}

bool ApplauncherProcess::startInstallation(
    const std::shared_ptr<applauncher::api::StartInstallationTask>& task,
    applauncher::api::StartInstallationResponse* const response)
{
    //TODO/IMPL if installation of this version is already running, returning id of running installation

    //if already installed, running restore
    //if( m_installationManager->isVersionInstalled(task->version) )
    //{
    //    response->result = applauncher::api::ResultType::alreadyInstalled;
    //    return true;
    //}

    //detecting directory to download to
    const QString& targetDir = m_installationManager->installationDirForVersion(task->version);
    if (!QDir().mkpath(targetDir))
    {
        response->result = applauncher::api::ResultType::ioError;
        return true;
    }

    std::shared_ptr<InstallationProcess> installationProcess(new InstallationProcess(
        QnAppInfo::productNameShort(),
        QnAppInfo::customizationName(),
        task->version,
        task->module,
        targetDir,
        task->autoStart));
    if (!installationProcess->start(m_mirrorListUrl))
    {
        response->result = applauncher::api::ResultType::ioError;
        return true;
    }

    response->installationID = ++m_prevInstallationID;
    m_activeInstallations.insert(std::make_pair(response->installationID, installationProcess));

    connect(installationProcess.get(), &InstallationProcess::installationDone,
            this, &ApplauncherProcess::onInstallationDone, Qt::DirectConnection);

    response->result = applauncher::api::ResultType::ok;
    return true;
}

bool ApplauncherProcess::getInstallationStatus(
    const std::shared_ptr<applauncher::api::GetInstallationStatusRequest>& request,
    applauncher::api::InstallationStatusResponse* const response)
{
    auto installationIter = m_activeInstallations.find(request->installationID);
    if (installationIter == m_activeInstallations.end())
    {
        response->result = applauncher::api::ResultType::notFound;
        return true;
    }

    response->status = installationIter->second->getStatus();
    response->progress = installationIter->second->getProgress();

    if (response->status > applauncher::api::InstallationStatus::cancelInProgress)
    {
        switch (response->status)
        {
            case applauncher::api::InstallationStatus::success:
                NX_LOG(QString::fromLatin1("Installation finished successfully"), cl_logDEBUG2);
                break;
            case applauncher::api::InstallationStatus::failed:
                NX_LOG(QString::fromLatin1("Installation has failed. %1").arg(installationIter->second->errorText()), cl_logDEBUG1);
                break;
            case applauncher::api::InstallationStatus::cancelled:
                NX_LOG(QString::fromLatin1("Installation has been cancelled"), cl_logDEBUG2);
                break;
            default:
                break;
        }
        m_activeInstallations.erase(installationIter);
    }

    return true;
}

bool ApplauncherProcess::installZip(
    const std::shared_ptr<applauncher::api::InstallZipTask>& request,
    applauncher::api::Response* const response)
{
    bool ok = m_installationManager->installZip(request->version, request->zipFileName);
    response->result = ok ? applauncher::api::ResultType::ok : applauncher::api::ResultType::otherError;
    return ok;
}

bool ApplauncherProcess::isVersionInstalled(
    const std::shared_ptr<applauncher::api::IsVersionInstalledRequest>& request,
    applauncher::api::IsVersionInstalledResponse* const response)
{
    response->installed = m_installationManager->isVersionInstalled(request->version);
    return true;
}

bool ApplauncherProcess::getInstalledVersions(
    const std::shared_ptr<applauncher::api::GetInstalledVersionsRequest>& request,
    applauncher::api::GetInstalledVersionsResponse* const response)
{
    Q_UNUSED(request)
        response->versions = m_installationManager->installedVersions();
    return true;
}

bool ApplauncherProcess::cancelInstallation(
    const std::shared_ptr<applauncher::api::CancelInstallationRequest>& request,
    applauncher::api::CancelInstallationResponse* const response)
{
    auto it = m_activeInstallations.find(request->installationID);
    if (it == m_activeInstallations.end())
    {
        response->result = applauncher::api::ResultType::otherError;
        return true;
    }

    it->second->cancel();

    response->result = applauncher::api::ResultType::ok;
    return true;
}

bool ApplauncherProcess::addProcessKillTimer(
    const std::shared_ptr<applauncher::api::AddProcessKillTimerRequest>& request,
    applauncher::api::AddProcessKillTimerResponse* const response)
{
    KillProcessTask task;
    task.processID = request->processID;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_terminated)
        {
            response->result = applauncher::api::ResultType::otherError;
            return true;
        }

        m_killProcessTasks[nx::utils::TimerManager::instance()->addTimer(
            this, std::chrono::milliseconds(request->timeoutMillis))] = task;
    }

    response->result = applauncher::api::ResultType::ok;
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

void ApplauncherProcess::onInstallationDone(InstallationProcess* installationProcess)
{
    if (installationProcess->getStatus() == applauncher::api::InstallationStatus::success)
    {
        m_installationManager->updateInstalledVersionsInformation();
        if (installationProcess->autoStartNeeded())
        {
            applauncher::api::Response response;
            startApplication(
                std::make_shared<applauncher::api::StartApplicationTask>(installationProcess->getVersion()),
                &response);
        }
    }
}

static const int INSTALLATION_CHECK_TIMEOUT_MS = 1000;

bool ApplauncherProcess::blockingRestoreVersion(const QnSoftwareVersion& versionToLaunch)
{
    //trying to restore installed version
    applauncher::api::StartInstallationResponse startInstallationResponse;
    if (!startInstallation(
        std::make_shared<applauncher::api::StartInstallationTask>(versionToLaunch),
        &startInstallationResponse))
    {
        return false;
    }

    applauncher::api::InstallationStatusResponse response;
    for (;; )
    {
        getInstallationStatus(
            std::make_shared<applauncher::api::GetInstallationStatusRequest>(startInstallationResponse.installationID),
            &response);

        if (response.status == applauncher::api::InstallationStatus::inProgress)
        {
            QThread::msleep(INSTALLATION_CHECK_TIMEOUT_MS);
            continue;
        }

        break;
    }

    return response.status == applauncher::api::InstallationStatus::success;
}
