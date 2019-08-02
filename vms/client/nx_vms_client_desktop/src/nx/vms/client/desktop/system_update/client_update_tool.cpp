#include "client_update_tool.h"

#include <QtCore/QCoreApplication>

#include <common/common_module.h>
#include <api/global_settings.h>
#include <api/server_rest_connection.h>
#include <utils/applauncher_utils.h>
#include <utils/common/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/update/update_check.h>
#include <nx/vms/common/p2p/downloader/private/resource_pool_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/internet_only_peer_manager.h>
#include <nx/vms/client/desktop/ini.h>

#include "update_verification.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::common::p2p::downloader;
using namespace nx::vms::applauncher::api;

bool requestInstalledVersions(QList<nx::utils::SoftwareVersion>* versions)
{
    // Try to run applauncher if it is not running.
    if (!checkOnline())
        return false;
    int kMaxTries = 5;
    do
    {
        if (getInstalledVersions(versions) == ResultType::ok)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    } while (--kMaxTries > 0);

    return false;
}

ClientUpdateTool::ClientUpdateTool(QObject *parent):
    base_type(parent),
    m_outputDir(QDir::temp().absoluteFilePath("nx_updates/client")),
    m_peerManager(new ResourcePoolPeerManager(commonModule())),
    m_proxyPeerManager(new ResourcePoolProxyPeerManager(commonModule()))
{
    // Expecting m_outputDir to be like /temp/nx_updates/client

    if (ini().massSystemUpdateClearDownloads)
        clearDownloadFolder();

    m_downloader.reset(new Downloader(
        m_outputDir,
        commonModule(),
        {m_peerManager, new InternetOnlyPeerManager(), m_proxyPeerManager}));

    connect(m_downloader.get(), &Downloader::fileStatusChanged,
        this, &ClientUpdateTool::atDownloaderStatusChanged);

    connect(m_downloader.get(), &Downloader::fileInformationChanged,
        this, &ClientUpdateTool::atDownloaderStatusChanged);

    connect(m_downloader.get(), &Downloader::downloadFinished,
        this, &ClientUpdateTool::atDownloadFinished);

    connect(m_downloader.get(), &Downloader::chunkDownloadFailed,
        this, &ClientUpdateTool::atChunkDownloadFailed);

    connect(m_downloader.get(), &Downloader::downloadFailed,
        this, &ClientUpdateTool::atDownloadFailed);

    connect(m_downloader.get(), &Downloader::downloadStalledChanged,
        this, &ClientUpdateTool::atDownloadStallChanged);

    m_downloader->startDownloads();

    m_installedVersionsFuture = std::async(std::launch::async,
        []()
        {
            std::set<nx::utils::SoftwareVersion> output;
            QList<nx::utils::SoftwareVersion> versions;
            if (requestInstalledVersions(&versions))
            {
                for (const auto& version: versions)
                    output.insert(version);
            }
            return output;
        });
}

ClientUpdateTool::~ClientUpdateTool()
{
    NX_VERBOSE(this, "~ClientUpdateTool() enter");

    // Making sure attached thread is complete
    m_state = exiting;
    if (m_applauncherTask.valid())
        m_applauncherTask.get();

    // Forcing downloader to be destroyed before serverConnection.
    if (m_downloader)
    {
        m_downloader->disconnect(this);
        m_downloader.reset();
    }

    m_serverConnection.reset();
    NX_VERBOSE(this, "~ClientUpdateTool() done");
}

void ClientUpdateTool::setState(State newState)
{
    if (m_state == newState)
        return;

    m_state = newState;
    m_stateChanged = true;
    m_lastError = QString();
    emit updateStateChanged((int)m_state, 0, {});
}

void ClientUpdateTool::setError(const QString& error)
{
    m_state = State::error;
    m_lastError = error;
    emit updateStateChanged((int)m_state, 0, error);
}

void ClientUpdateTool::setApplauncherError(const QString& error)
{
    m_state = State::applauncherError;
    m_lastError = error;
    emit updateStateChanged((int)m_state, 0, error);
}

QString ClientUpdateTool::getErrorText() const
{
    return m_lastError;
}

std::future<nx::update::UpdateContents> ClientUpdateTool::requestInstalledUpdateInfo()
{
    if (!m_serverConnection)
    {
        NX_WARNING(this, "requestInstalledUpdateInfo() - have no connection to the server");
        std::promise<UpdateContents> promise;
        promise.set_value(UpdateContents());
        return promise.get_future();
    }

    auto promise = std::make_shared<std::promise<UpdateContents>>();
    // Requesting remote update info.
    // NOTE: This can be a 3.2 system, so we can fail this step completely.
    m_serverConnection->getInstalledUpdateInfo(
        [promise](bool success, rest::Handle /*handle*/, rest::UpdateInformationData response)
        {
            nx::update::InformationError error = nx::update::InformationError::noError;
            if (!success || response.error != QnRestResult::NoError)
            {
                NX_DEBUG(typeid(UpdateContents),
                    "requestInstalledUpdateInfo() - Error in response for /updateInformation request: %1",
                    response.errorString);
                if (!QnLexical::deserialize(response.errorString, &error))
                    error = nx::update::InformationError::httpError;
            }

            UpdateContents contents;
            contents.sourceType = nx::update::UpdateSourceType::mediaservers;
            contents.source = "mediaserver";
            contents.info = response.data;
            contents.error = error;
            promise->set_value(contents);
        });
    return promise->get_future();
}

std::future<nx::update::UpdateContents> ClientUpdateTool::requestInternetUpdateInfo(
    const QString& updateUrl,
    const QString& changeset)
{
    auto engineVersion = commonModule()->engineVersion();

    if (!m_serverConnection)
        NX_WARNING(this, "requestInternetUpdateInfo() - have no connection to the server");

    return std::async(
        [updateUrl, connection = m_serverConnection, engineVersion, changeset]()
        {
            auto contents = nx::update::checkSpecificChangesetProxied(
                connection, engineVersion, updateUrl, changeset);
            contents.sourceType = nx::update::UpdateSourceType::internetSpecific;
            return contents;
        });
}

void ClientUpdateTool::setServerUrl(const nx::utils::Url& serverUrl, const QnUuid& serverId)
{
    m_serverConnection.reset(new rest::ServerConnection(commonModule(), serverId, serverUrl));
    m_peerManager->setServerDirectConnection(serverId, m_serverConnection);
    m_proxyPeerManager->setServerDirectConnection(serverId, m_serverConnection);
}

std::set<nx::utils::SoftwareVersion> ClientUpdateTool::getInstalledClientVersions(
    bool includeCurrentVersion) const
{
    if (m_installedVersionsFuture.valid())
        m_installedVersions = m_installedVersionsFuture.get();
    auto result = m_installedVersions;
    if (includeCurrentVersion)
    {
        QString clientVersion = nx::utils::AppInfo::applicationVersion();
        result.insert(nx::utils::SoftwareVersion(clientVersion));
    }
    return result;
}

bool ClientUpdateTool::isVersionInstalled(const nx::utils::SoftwareVersion& version) const
{
    auto versions = getInstalledClientVersions(/*includeCurrentVersion=*/true);
    return versions.count(version) != 0;
}

bool ClientUpdateTool::shouldInstallThis(const UpdateContents& contents) const
{
    if (!contents.clientPackage.isValid())
        return false;

    auto version = contents.getVersion();
    auto installedVersions = getInstalledClientVersions(true);

    return installedVersions.count(version) == 0;
}

void ClientUpdateTool::setUpdateTarget(const UpdateContents& contents)
{
    m_clientPackage = contents.clientPackage;
    m_updateVersion = contents.getVersion();
    if (!contents.needClientUpdate)
    {
        NX_INFO(this, "setUpdateTarget(%1) - no need to install this version",
            contents.info.version);
        setState(State::complete);
        return;
    }

    if (isVersionInstalled(m_updateVersion))
    {
        if (shouldRestartTo(m_updateVersion))
        {
            NX_INFO(this)
                << "setUpdateTarget(" << contents.info.version
                << ") client already has this version installed"
                << m_clientPackage.localFile;
            setState(State::readyRestart);
        }
        else
        {
            NX_INFO(this)
                << "setUpdateTarget(" << contents.info.version
                << ") client is already at this version"
                << m_clientPackage.localFile;
            setState(State::complete);
        }
    }
    else if (contents.sourceType == nx::update::UpdateSourceType::file)
    {
        // Expecting that file is stored at:
        NX_INFO(this)
            << "setUpdateTarget(" << contents.info.version << ") this is offline update from the file"
            << m_clientPackage.localFile;
        QString path = contents.storageDir.filePath(m_clientPackage.localFile);
        if (!QFileInfo::exists(path))
        {
            NX_ERROR(this)
                << "setUpdateTarget(" << contents.info.version << ") the file"
                << path << "does not exist!";
            setError(QString("File %1 does not exist").arg(path));
        }
        else
        {
            m_updateFile = path;
            setState(State::readyInstall);
        }
    }
    else
    {
        NX_INFO(this, "setUpdateTarget(%1) this is an internet update", contents.info.version);

        FileInformation info;
        info.md5 = QByteArray::fromHex(m_clientPackage.md5.toLatin1());
        info.size = m_clientPackage.size;
        info.name = m_clientPackage.file;
        info.url = m_clientPackage.url;

        if (!info.isValid())
        {
            setError("There is no valid client package to download");
            return;
        }

        setState(State::downloading);

        const auto code = m_downloader->addFile(info);
        NX_VERBOSE(this, "setUpdateTarget(%1) m_downloader->addFile code=%2", contents.info.version, code);

        if (code == common::p2p::downloader::ResultCode::fileAlreadyExists
            || code == common::p2p::downloader::ResultCode::fileAlreadyDownloaded)
        {
            // Forcing downloader to start processing this file.
            // It should call all the events and ClientUpdateTool will process its state.
            m_downloader->startDownloads();
        }
        else if (code != common::p2p::downloader::ResultCode::ok)
        {
            const QString error = common::p2p::downloader::toString(code);
            NX_ERROR(this, "setUpdateTarget() - failed to add client package %1 %2",
                info.name, error);
            setError(error);
            return;
        }
    }
}

void ClientUpdateTool::atDownloaderStatusChanged(const FileInformation& fileInformation)
{
    if (fileInformation.name != m_clientPackage.file)
        return;

    if (m_state != State::downloading)
    {
        // WTF are we doing here?
        return;
    }

    switch (fileInformation.status)
    {
        case FileInformation::Status::notFound:
            setError(tr("Update file is not found"));
            break;
        case FileInformation::Status::uploading:
            break;
        case FileInformation::Status::downloaded:
            atDownloadFinished(fileInformation.name);
            break;
        case FileInformation::Status::corrupted:
            setError(tr("Update package is corrupted"));
            break;
        case FileInformation::Status::downloading:
            emit updateStateChanged(int(State::downloading),
                fileInformation.calculateDownloadProgress(), {});
            break;
        default:
            // Nothing to do here
            break;
    }
}

void ClientUpdateTool::atDownloadFinished(const QString& fileName)
{
    if (fileName != m_clientPackage.file)
        return;

    m_updateFile = m_downloader->filePath(fileName);

    NX_VERBOSE(this, "atDownloadFinished(%1) - finally downloaded file to %2",
        fileName, m_updateFile);
    setState(State::readyInstall);
}

void ClientUpdateTool::atChunkDownloadFailed(const QString& /*fileName*/)
{
    // It is a breakpoint catcher.
    //NX_VERBOSE(this) << "atChunkDownloadFailed() failed to download chunk for" << fileName;
}

void ClientUpdateTool::atDownloadFailed(const QString& fileName)
{
    if (m_state == State::downloading)
    {
        NX_ERROR(this, "atDownloadFailed() failed to download file %1", fileName);
        setError(tr("Failed to download update package: %1").arg(fileName));
    }
}

void ClientUpdateTool::atDownloadStallChanged(const QString& fileName, bool stalled)
{
    if (m_state == State::downloading && stalled)
    {
        NX_ERROR(this, "atDownloadFailed() download of %1 has stalled", fileName);
        setError(tr("Failed to download update package: %1").arg(fileName));
    }
}

void ClientUpdateTool::atExtractFilesFinished(int code)
{
    if (code != QnZipExtractor::Ok)
    {
        QString error = QnZipExtractor::errorToString((QnZipExtractor::Error)code);
        NX_ERROR(this, "atExtractFilesFinished() err=%1",  error);
        setError(tr("Update package is corrupted: %1").arg(error));
        return;
    }

    NX_VERBOSE(this, "at_extractFilesFinished() done unpacking file");
    setState(State::readyInstall);
}

bool ClientUpdateTool::isDownloadComplete() const
{
    return !hasUpdate()
        || m_state == State::readyInstall
        || m_state == State::readyRestart
        || m_state == State::complete;
}

void ClientUpdateTool::checkInternalState()
{
    using nx::vms::applauncher::api::ResultType;

    auto kWaitTime = std::chrono::milliseconds(1);
    if (m_applauncherTask.valid()
        && m_applauncherTask.wait_for(kWaitTime) == std::future_status::ready)
    {
        const ResultType result = m_applauncherTask.get();
        bool shouldRestart = shouldRestartTo(m_updateVersion);

        switch (result)
        {
            case ResultType::alreadyInstalled:
            case ResultType::ok:
                if (shouldRestart)
                    setState(readyRestart);
                else
                    setState(complete);
                break;

            case ResultType::otherError:
            case ResultType::versionNotInstalled:
            case ResultType::invalidVersionFormat:
            case ResultType::notEnoughSpace:
            case ResultType::notFound:
            case ResultType::ioError:
            {
                QString error = applauncherErrorToString(result);
                NX_ERROR(this) << "Failed check installation:" << error;
                setApplauncherError(error);
                break;
            }
            default:
                break;
        }
    }
}

bool ClientUpdateTool::installUpdateAsync()
{
    if (m_state != readyInstall)
        return false;
    // Try to run applauncher if it is not running.
    if (!applauncher::api::checkOnline())
    {
        NX_VERBOSE(this) << "installUpdate can not install update - applauncher is offline" << error;
        auto errorText = applauncherErrorToString(ResultType::otherError);
        setApplauncherError(errorText);
        return false;
    }

    NX_INFO(this, "installUpdateAsync() from file %1", m_updateFile);
    NX_ASSERT(!m_updateFile.isEmpty());

    setState(installing);

    m_applauncherTask = std::async(std::launch::async,
        [tool = QPointer(this)](
            QString updateFile,
            nx::utils::SoftwareVersion updateVersion) -> applauncher::api::ResultType
        {
            QString absolutePath = QFileInfo(updateFile).absoluteFilePath();

            int installationAttempts = 2;
            bool stopInstallationAttempts = false;
            // We will try several installation attempts. It will solve a problem, when
            // applauncher was restarted during zip installation.
            do
            {
                const ResultType result = installZipAsync(updateVersion, absolutePath);
                if (result != ResultType::ok)
                {
                    const QString message = applauncherErrorToString(result);
                    NX_ERROR(NX_SCOPE_TAG, "Failed to start async zip installation: %1", message);
                    // Other variants can be fixed by retrying installation, do they?
                    return result;
                }

                NX_VERBOSE(NX_SCOPE_TAG,
                    "Started client installation from file %2. Waiting for completion", absolutePath);

                // Checking state if installation, until it goes to Result::ok
                constexpr int kMaxTries = 60;
                for (int retries = 0; retries < kMaxTries; ++retries)
                {
                    bool stopCheck = false;
                    applauncher::api::InstallationProgress progress;
                    const ResultType result = applauncher::api::checkInstallationProgress(
                        updateVersion, progress);
                    QString message = applauncherErrorToString(result);

                    switch (result)
                    {
                        case ResultType::versionNotInstalled:
                            NX_VERBOSE(NX_SCOPE_TAG,
                            "checkInstallationProgress - installation failed. Retrying");
                            stopCheck = true;
                            break;
                        case ResultType::alreadyInstalled:
                        case ResultType::invalidVersionFormat:
                        case ResultType::notEnoughSpace:
                        case ResultType::notFound:
                        case ResultType::ok:
                            NX_VERBOSE(NX_SCOPE_TAG,
                                "checkInstallationProgress returned %1. Exiting", message);
                            return result;
                        case ResultType::unpackingZip:
                            NX_VERBOSE(NX_SCOPE_TAG, "checkInstallationProgress() %1 of %2 unpacked",
                                progress.extracted, progress.total);
                            break;
                        case ResultType::otherError:
                        case ResultType::connectError:
                        case ResultType::ioError:
                        default:
                            NX_ERROR(NX_SCOPE_TAG, "checkInstallationProgress() failed to check zip "
                                "installation status: %1", message);
                            break;
                    }

                    // We can spend a lot of time in this cycle. So we should be able to exit
                    // as early as possible.
                    if (!tool || tool->m_state != installing)
                    {
                        NX_ERROR(NX_SCOPE_TAG, "checkInstallationProgress() is interrupted. Exiting");
                        stopInstallationAttempts = true;
                        break;
                    }

                    if (stopCheck)
                        break;

                    int percent = progress.total != 0 ? 100 * progress.extracted / progress.total : 0;
                    emit tool->updateStateChanged(tool->m_state, percent, {});
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
                if (stopInstallationAttempts)
                    break;
            } while (--installationAttempts > 0);

            return ResultType::otherError;
        }, m_updateFile, m_updateVersion);
    return false;
}

bool ClientUpdateTool::isInstallComplete() const
{
    switch (m_state)
    {
        case State::pendingUpdateInfo:
        case State::readyDownload:
        case State::readyInstall:
        case State::downloading:
            return false;
        case State::installing:
            // We need to check applauncher to get a proper result
            break;
        case State::readyRestart:
        case State::complete:
        case State::initial:
        case State::exiting:
        // Though actual install is not successful, no further progress is possible.
        case State::error:
        case State::applauncherError:
            return true;
    }

    bool installed = false;
    const ResultType result = applauncher::api::isVersionInstalled(
        m_updateVersion,
        &installed);

    switch (result)
    {
        case ResultType::alreadyInstalled:
            break;
        case ResultType::ok:
            return true;

        case ResultType::otherError:
        case ResultType::versionNotInstalled:
        case ResultType::invalidVersionFormat:
        {
            QString error = applauncherErrorToString(result);
            NX_ERROR(this) << "Failed to check installation:" << error;
            //setApplauncherError(error);
            return false;
        }
        default:
            // Other variats can be fixed by retrying installation, do they?
            break;
    }

    return installed;
}

bool ClientUpdateTool::shouldRestartTo(const nx::utils::SoftwareVersion& version) const
{
    QString clientVersion = nx::utils::AppInfo::applicationVersion();
    return version != nx::utils::SoftwareVersion(clientVersion);
}

bool ClientUpdateTool::restartClient(QString authString)
{
    /* Try to run applauncher if it is not running. */
    if (!applauncher::api::checkOnline())
        return false;

    const ResultType result = applauncher::api::restartClient(m_updateVersion, authString);
    if (result == ResultType::ok)
        return true;

    constexpr int kMaxTries = 5;
    for (int i = 0; i < kMaxTries; ++i)
    {
        QThread::msleep(200);
        qApp->processEvents();
        if (applauncher::api::restartClient(m_updateVersion, authString) == ResultType::ok)
            return true;
    }

    NX_ERROR(this, "restartClient() - failed to restart client to desired version %1",
        m_updateVersion);
    return false;
}

void ClientUpdateTool::resetState()
{
    NX_DEBUG(this, "resetState() from state %1", toString(m_state));
    switch (m_state)
    {
        case State::downloading:
            m_downloader->deleteFile(m_clientPackage.file, false);
            break;
        case State::readyInstall:
            break;
        case State::initial:
        default:
            // Nothing to do here.
            break;
    }

    m_state = State::initial;
    m_lastError = "";
    m_updateFile = "";
    m_updateVersion = nx::utils::SoftwareVersion();
    m_clientPackage = nx::update::Package();
}

void ClientUpdateTool::clearDownloadFolder()
{
    // Clear existing folder for downloaded update files.
    m_outputDir.removeRecursively();
    if (!m_outputDir.exists())
        m_outputDir.mkpath(".");
}

ClientUpdateTool::State ClientUpdateTool::getState() const
{
    return m_state;
}

bool ClientUpdateTool::hasUpdate() const
{
    return m_state != State::initial
        && m_state != State::error
        && m_state != State::applauncherError
        && m_state != State::complete;
}

QString ClientUpdateTool::toString(State state)
{
    switch (state)
    {
        case State::initial:
            return "Initial";
        case State::pendingUpdateInfo:
            return "PendingUpdateInfo";
        case State::readyDownload:
            return "ReadyDownload";
        case State::downloading:
            return "Downloading";
        case State::readyInstall:
            return "ReadyInstall";
        case State::installing:
            return "Installing";
        case State::readyRestart:
            return "ReadyRestart";
        case State::complete:
            return "Complete";
        case State::exiting:
            return "Exiting";
        case State::error:
            return "Error";
        case State::applauncherError:
            return "applauncherError";
    }
    return QString();
}

QString ClientUpdateTool::applauncherErrorToString(ResultType value)
{
    switch (value)
    {
        case ResultType::alreadyInstalled:
            return tr("This update is already installed.");
        case ResultType::otherError:
            return tr("Internal error.");
        case ResultType::versionNotInstalled:
            return tr("This version is not installed.");
        case ResultType::invalidVersionFormat:
            return tr("Invalid version format.");
        case ResultType::brokenPackage:
            return tr("Broken update package.");
        case ResultType::notEnoughSpace:
            return tr("Not enough space on disk to install the client update.");
        case ResultType::notFound:
            // Installed package does not exists. Either we have broken the code and asking
            // for a wrong file, or this file had been removed somehow.
            return tr("Installation package has been lost.");
        default:
            return nx::vms::applauncher::api::toString(value);
    }
}

} // namespace nx::vms::client::desktop
