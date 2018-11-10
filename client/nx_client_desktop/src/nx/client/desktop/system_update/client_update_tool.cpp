#include <utils/applauncher_utils.h>
#include <nx/utils/log/log.h>
#include <api/server_rest_connection.h>

#include "nx/vms/common/p2p/downloader/private/single_connection_peer_manager.h"
#include "client_update_tool.h"

namespace nx {
namespace client {
namespace desktop {

ClientUpdateTool::ClientUpdateTool(QObject *parent):
    base_type(parent),
    m_outputDir(QDir::temp().absoluteFilePath("nx_updates/client"))
{
    // Expecting m_outputDir to be like /temp/nx_updates/client

    vms::common::p2p::downloader::AbstractPeerSelectorPtr peerSelector;
    m_peerManager.reset(new SingleConnectionPeerManager(commonModule(), std::move(peerSelector)));
    m_peerManager->setParent(this);

    m_downloader.reset(new Downloader(m_outputDir, commonModule(), this));
    connect(m_downloader.get(), &Downloader::fileStatusChanged,
        this, &ClientUpdateTool::atDownloaderStatusChanged);

    connect(m_downloader.get(), &Downloader::chunkDownloadFailed,
        this, &ClientUpdateTool::atChunkDownloadFailed);
}

ClientUpdateTool::~ClientUpdateTool()
{
    m_downloader->disconnect(this);
    m_serverConnection.reset();
    m_peerManager.reset();
}

void ClientUpdateTool::setState(State newState)
{
    if (m_state == newState)
        return;

    m_state = newState;
    m_stateChanged = true;
    m_lastError = QString();
    emit updateStateChanged((int)m_state, 0);
}

void ClientUpdateTool::setError(QString error)
{
    m_state = State::error;
    m_lastError = error;
}

void ClientUpdateTool::setApplauncherError(QString error)
{
    m_state = State::applauncherError;
    m_lastError = error;
}

std::future<UpdateContents> ClientUpdateTool::requestRemoteUpdateInfo()
{
    m_remoteUpdateInfoRequest = std::promise<UpdateContents>();

    if (m_serverConnection)
    {
        // Requesting remote update info.
        m_serverConnection->getUpdateInfo(
            [tool=QPointer<ClientUpdateTool>(this)](bool success, rest::Handle handle, const nx::update::Information& response)
            {
                if (tool && success)
                    tool->atRemoteUpdateInformation(response);
            }, thread());
    }
    else
    {
        NX_WARNING(this) << "requestRemoteUpdateInfo() - have no connection to the server";

        m_remoteUpdateInfoRequest.set_value(UpdateContents());
        return m_remoteUpdateInfoRequest.get_future();
    }
    return m_remoteUpdateInfoRequest.get_future();
}

void ClientUpdateTool::setServerUrl(nx::utils::Url serverUrl, QnUuid serverId)
{
    m_serverConnection.reset(new rest::ServerConnection(commonModule(), serverId, serverUrl));
    m_peerManager->setServerUrl(serverUrl, serverId);
}

void ClientUpdateTool::atRemoteUpdateInformation(const nx::update::Information& updateInformation)
{
    auto clientPackage = findClientPackage(updateInformation);

    if (getState() == State::initial)
    {
        UpdateContents contents;
        contents.sourceType = UpdateSourceType::mediaservers;
        contents.source = "mediaserver";
        contents.info = updateInformation;
        contents.clientPackage = clientPackage;
        m_remoteUpdateContents = contents;

        if (clientPackage.isValid())
        {
            setState(State::readyDownload);
        }
        else if (updateInformation.isValid())
        {
            NX_WARNING(this) << "atRemoteUpdateInformation have valid update info but no client package";
            setError("Missing client package inside UpdateInfo");
        }

        m_remoteUpdateInfoRequest.set_value(contents);
    }
}

UpdateContents ClientUpdateTool::getRemoteUpdateInfo() const
{
    return m_remoteUpdateContents;
}

void ClientUpdateTool::downloadUpdate(const UpdateContents& contents)
{
    NX_VERBOSE(this) << "downloadUpdate() ver" << contents.info.version;
    m_clientPackage = contents.clientPackage;
    NX_ASSERT(m_clientPackage.isValid());

    m_updateVersion = nx::utils::SoftwareVersion(contents.info.version);

    if (contents.sourceType == UpdateSourceType::file)
    {
        // Expecting that file is stored at:
        QString path = contents.storageDir.filePath(m_clientPackage.file);
        m_updateFile = path;
        setState(State::readyInstall);
    }
    else
    {
        FileInformation info;
        info.md5 = QByteArray::fromHex(m_clientPackage.md5.toLatin1());
        info.size = m_clientPackage.size;
        info.name = m_clientPackage.file;
        info.url = m_clientPackage.url;
        NX_ASSERT(info.isValid());
        auto code = m_downloader->addFile(info);
        using Code = vms::common::p2p::downloader::ResultCode;
        m_updateFile =  m_downloader->filePath(m_clientPackage.file);

        switch (code)
        {
            case Code::ok:
                NX_VERBOSE(this) << "requestStartUpdate() - downloading client package"
                    << info.name << " from url="<<m_clientPackage.url;
                setState(State::downloading);
                break;
            case Code::fileAlreadyExists:
                NX_VERBOSE(this) << "requestStartUpdate() - file is already here"
                    << info.name;
                setState(State::readyInstall);
                break;
            default:
            // Some sort of an error here.
            {
                QString error = vms::common::p2p::downloader::toString(code);
                NX_VERBOSE(this) << "requestStartUpdate() - failed to add client package "
                    << info.name << error;
                setError(error);
                break;
            }
        }
    }
}

void ClientUpdateTool::atDownloaderStatusChanged(const FileInformation& fileInformation)
{
    if (fileInformation.name != m_clientPackage.file)
        return;

    NX_VERBOSE(this) << "at_downloaderStatusChanged("<< fileInformation.name
        << ") - status changed to " << fileInformation.status;

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
            setState(State::readyInstall);
            break;
        case FileInformation::Status::corrupted:
            setError(tr("Update package is corrupted"));
            break;
        case FileInformation::Status::downloading:
            m_progress = fileInformation.calculateDownloadProgress();
            emit updateStateChanged(int(FileInformation::Status::downloading), m_progress);
            break;
        default:
            // Nothing to do here
            break;
    }
}

void ClientUpdateTool::atChunkDownloadFailed(const QString& fileName)
{
    // It is a breakpoint catcher.
    //NX_VERBOSE(this) << "atChunkDownloadFailed() failed to download chunk for" << fileName;
    //setError(tr("Update package is corrupted: %1").arg(error));
}

void ClientUpdateTool::atExtractFilesFinished(int code)
{
    if (code != QnZipExtractor::Ok)
    {
        QString error = QnZipExtractor::errorToString((QnZipExtractor::Error)code);
        NX_VERBOSE(this) << "atExtractFilesFinished() err=" << error;
        setError(tr("Update package is corrupted: %1").arg(error));
        return;
    }
    NX_VERBOSE(this) << "at_extractFilesFinished() done unpacking file" << error;
    setState(State::readyInstall);
}

int ClientUpdateTool::getDownloadProgress() const
{
    if (m_state == State::readyInstall)
        return 100;
    return m_progress;
}

bool ClientUpdateTool::isDownloadComplete() const
{
    return !hasUpdate() || m_state == State::readyInstall;
}

bool ClientUpdateTool::installUpdate()
{
    // Try to run applauncher if it is not running.
    if (!applauncher::api::checkOnline())
    {
        NX_VERBOSE(this) << "installUpdate can not install update - applauncher is offline" << error;
        setApplauncherError("applauncher is offline");
        return false;
    }

    NX_ASSERT(!m_updateFile.isEmpty());

    static const int kMaxTries = 5;
    QString absolutePath = QFileInfo(m_updateFile).absoluteFilePath();

    for (int retries = 0; retries < kMaxTries; retries++)
    {
        using Result = applauncher::api::ResultType::Value;
        Result result = applauncher::api::installZip(m_updateVersion, absolutePath);

        switch (result)
        {
            case Result::alreadyInstalled:
                setState(State::complete);
                return true;

            case Result::ok:
                setState(State::installing);
                return true;

            case Result::otherError:
            case Result::versionNotInstalled:
            case Result::invalidVersionFormat:
            {
                QString error = applauncher::api::ResultType::toString(result);
                NX_ERROR(this) << "Failed to run installation:" << error;
                setApplauncherError(error);
                return false;
            }
            default:
                // Other variats can be fixed by retrying installation, do they?
                break;
        }

        QThread::msleep(100);
        qApp->processEvents();
    }
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
        case State::complete:
        case State::initial:
        // Though actual install is not successful, no further progress is possible.
        case State::error:
        case State::applauncherError:
            return true;
    }

    bool installed = false;
    using Result = applauncher::api::ResultType::Value;
    Result result = applauncher::api::isVersionInstalled(m_updateVersion, &installed);

    switch (result)
    {
        case Result::alreadyInstalled:
            break;
        case Result::ok:
            return true;

        case Result::otherError:
        case Result::versionNotInstalled:
        case Result::invalidVersionFormat:
        {
            QString error = applauncher::api::ResultType::toString(result);
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

bool ClientUpdateTool::restartClient()
{
    /* Try to run applauncher if it is not running. */
    if (!applauncher::api::checkOnline())
        return false;

    using Result = applauncher::api::ResultType::Value;
    Result result = applauncher::api::restartClient(m_updateVersion);
    if (result == Result::ok)
        return true;

    static const int kMaxTries = 5;
    for (int i = 0; i < kMaxTries; ++i)
    {
        QThread::msleep(100);
        qApp->processEvents();
        if (applauncher::api::restartClient(m_updateVersion) == Result::ok)
            return true;
    }
    return false;
}

void ClientUpdateTool::resetState()
{
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
    m_progress = 0;
    m_updateVersion = nx::utils::SoftwareVersion();
    m_clientPackage = nx::update::Package();
    m_remoteUpdateContents = UpdateContents();
}

ClientUpdateTool::State ClientUpdateTool::getState() const
{
    return m_state;
}

bool ClientUpdateTool::hasUpdate() const
{
    return m_state != State::initial && m_state != State::error && m_state != State::applauncherError;
}

ClientUpdateTool::PeerManagerPtr ClientUpdateTool::createPeerManager(
    FileInformation::PeerSelectionPolicy /*peerPolicy*/, const QList<QnUuid>& /*additionalPeers*/)
{
    return m_peerManager.get();
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
        case State::complete:
            return "Complete";
        case State::error:
            return "Error";
        case State::applauncherError:
            return "applauncherError";
    }
    return QString();
}

} // namespace desktop
} // namespace client
} // namespace nx
